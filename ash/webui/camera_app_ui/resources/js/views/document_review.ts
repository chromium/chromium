// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {
  assert,
  assertExists,
  assertInstanceof,
  assertNotReached,
} from '../assert.js';
import * as dom from '../dom.js';
import {Point} from '../geometry.js';
import {I18nString} from '../i18n_string.js';
import {
  DocScanActionType,
  DocScanFixType,
  DocScanResultActionType,
  sendDocScanEvent,
  sendDocScanResultEvent,
} from '../metrics.js';
import {Filenamer} from '../models/file_namer.js';
import {getI18nMessage} from '../models/load_time_data.js';
import {ResultSaver} from '../models/result_saver.js';
import {
  ChromeHelper,
  createBigBufferFromBlob,
  createNumArrayFromBlob,
  handleBigBufferError,
  shouldUseBigBuffer,
} from '../mojo/chrome_helper.js';
import {
  BigBuffer,
  PdfBuilderRemote,
} from '../mojo/type.js';
import * as nav from '../nav.js';
import {PerfLogger} from '../perf.js';
import {speakMessage} from '../spoken_msg.js';
import {show as showToast} from '../toast.js';
import {
  MimeType,
  PerfEvent,
  Rotation,
  ViewName,
} from '../type.js';
import {
  getKeyboardShortcut,
  instantiateTemplate,
  KeyboardShortcut,
  loadImage,
  share,
} from '../util.js';

import {DocumentFixMode} from './document_fix_mode.js';
import {DocumentPreviewMode} from './document_preview_mode.js';
import {View} from './view.js';

export interface Page {
  blob: Blob;
  corners: Point[];
  rotation: Rotation;
}
interface PageInternal extends Page {
  isCornersUpdated: boolean;
  isRotationUpdated: boolean;
  croppedBlob: Blob;
  isDirty: boolean;
}

export enum Mode {
  FIX,
  PREVIEW,
}

// The class to set on page element when the page is selected.
const ACTIVE_PAGE_CLASS = 'active';
const DELETE_PAGE_BUTTON_SELECTOR = '.delete';

// The initialized `DocumentReview` instance.
let instance: DocumentReview|null = null;

/**
 * Initialize the `DocumentReview` instance. It should only be initialize once.
 */
export function initializeInstance(resultSaver: ResultSaver): DocumentReview {
  assert(instance === null);
  instance = new DocumentReview(resultSaver);
  return instance;
}

/**
 * Get the `DocumentReview` instance for testing purpose.
 */
export function getInstanceForTest(): DocumentReview {
  return assertExists(instance);
}

/**
 * View controller for reviewing document scanning.
 */
export class DocumentReview extends View {
  /**
   * Information of each pages for creating PDF files and sending metrics.
   */
  private pages: PageInternal[] = [];

  /**
   * The sidebar element, which contains the thumbnails and delete buttons of
   * each page.
   */
  private readonly pagesElement: HTMLDivElement;

  /**
   * The container to render the view of current active mode.
   */
  private readonly previewElement: HTMLDivElement;

  /**
   * The index of the current active page.
   */
  private selectedIndex = -1;

  /**
   * The current active mode.
   */
  private mode = Mode.PREVIEW;

  private readonly modes: {
    [Mode.FIX]: DocumentFixMode,
    [Mode.PREVIEW]: DocumentPreviewMode,
  };

  /**
   * The promise of current page updating process. Null if no pages are being
   * updated.
   */
  private updatingPage: Promise<void>|null = null;

  /**
   * Pending payload for updating pages. Null if there isn't any pending
   * payload.
   */
  private pendingUpdatePayload: [number, PageInternal]|null = null;

  /**
   * Count the fix times of each session (reset when page count is zero) for
   * sending events.
   */
  private fixCount = 0;

  /**
   * The processing time of last saved file.
   */
  private lastFileProcessingTime: number|null = null;

  /**
   * The interface to build PDFs.
   */
  private readonly pdfBuilder = new PdfBuilder();

  constructor(protected readonly resultSaver: ResultSaver) {
    super(ViewName.DOCUMENT_REVIEW, {
      defaultFocusSelector: '.show .primary',
    });
    this.pagesElement =
        dom.getFrom(this.root, '.document-pages', HTMLDivElement);
    this.previewElement =
        dom.getFrom(this.root, '.document-preview', HTMLDivElement);
    this.pagesElement.addEventListener('keydown', (e) => {
      const key = getKeyboardShortcut(e);
      if (key === ' ') {
        const target = assertInstanceof(e.target, HTMLElement);
        target.click();
      }
    });
    this.pagesElement.addEventListener('click', async (e: MouseEvent) => {
      const target = assertInstanceof(e.target, HTMLElement);
      const pageElement = target.closest('.page');
      if (pageElement === null) {
        return;
      }
      const index = Array.from(this.pagesElement.children).indexOf(pageElement);
      const clickOnDeleteButton =
          target.closest(DELETE_PAGE_BUTTON_SELECTOR) !== null;
      if (clickOnDeleteButton) {
        await this.onDeletePage(index);
        return;
      }
      await this.onSelectPage(index);
    });

    const pagesElementMutationObserver = new MutationObserver((mutations) => {
      for (const mutation of mutations) {
        assert(
            mutation.type === 'childList',
            '`mutation.type` must be `childList`');
        this.updateDeleteButtonLabels();
      }
    });
    pagesElementMutationObserver.observe(this.pagesElement, {childList: true});

    const fixMode = new DocumentFixMode({
      target: this.previewElement,
      onDone: async () => {
        await this.waitForUpdatingPage(() => this.showMode(Mode.PREVIEW));
        for (const [index, page] of this.pages.entries()) {
          if (page.isDirty) {
            void this.pdfBuilder.addPage(page.croppedBlob, index);
            page.isDirty = false;
          }
        }
      },
      onUpdatePage: async ({corners, rotation}) => {
        const page = this.pages[this.selectedIndex];
        const isCornersUpdated = page.isCornersUpdated ||
            page.corners.some(
                (oldCorner, i) => oldCorner.x !== corners[i].x ||
                    oldCorner.y !== corners[i].y);
        const isRotationUpdated =
            page.isRotationUpdated || page.rotation !== rotation;
        await this.updatePage(this.selectedIndex, {
          ...page,
          corners,
          rotation,
          isCornersUpdated,
          isRotationUpdated,
        });
      },
      onShow: () => {
        this.fixCount += 1;
      },
    });
    const previewMode = new DocumentPreviewMode({
      target: this.previewElement,
      onAdd: () => {
        sendDocScanEvent(DocScanActionType.ADD_PAGE);
        this.close();
      },
      onCancel: () => {
        this.sendResultEvent(DocScanResultActionType.CANCEL);
        this.clearPages();
        this.close();
      },
      onFix: async () => {
        sendDocScanEvent(DocScanActionType.FIX);
        await this.showMode(Mode.FIX);
      },
      onShare: async () => {
        this.sendResultEvent(DocScanResultActionType.SHARE);
        await this.share(
            this.pages.length > 1 ? MimeType.PDF : MimeType.JPEG,
        );
      },
      onSave: async (mimeType: MimeType.JPEG|MimeType.PDF) => {
        const perfLogger = PerfLogger.getInstance();
        if (mimeType === MimeType.PDF) {
          perfLogger.start(PerfEvent.DOCUMENT_PDF_SAVING);
        }
        this.sendResultEvent(
            mimeType === MimeType.JPEG ? DocScanResultActionType.SAVE_AS_PHOTO :
                                         DocScanResultActionType.SAVE_AS_PDF);
        nav.open(ViewName.FLASH);
        let hasError = false;
        const pageCount = this.pages.length;
        try {
          await this.save(mimeType);
          this.clearPages();
          this.close();
        } catch (e) {
          hasError = true;
          showToast(I18nString.ERROR_MSG_SAVE_FILE_FAILED);
        } finally {
          nav.close(ViewName.FLASH);
          if (mimeType === MimeType.PDF) {
            perfLogger.stop(
                PerfEvent.DOCUMENT_PDF_SAVING, {hasError, pageCount});
          }
        }
      },
    });
    this.modes = {
      [Mode.FIX]: fixMode,
      [Mode.PREVIEW]: previewMode,
    };
    previewMode.show();
  }

  /**
   * Adds a page to `this.pages` and updates related elements.
   */
  async addPage(page: Page): Promise<void> {
    const {blob: croppedBlob} = await this.crop(page);
    const pageInternal: PageInternal = {
      ...page,
      isCornersUpdated: false,
      isRotationUpdated: false,
      croppedBlob,
      isDirty: false,
    };
    await this.addPageView(croppedBlob);
    this.pages.push(pageInternal);
    if (this.pages.length === 1) {
      this.pdfBuilder.create();
    }
    void this.pdfBuilder.addPage(croppedBlob, this.pages.length - 1);
  }

  private async addPageView(blob: Blob): Promise<void> {
    const fragment = instantiateTemplate('#document-review-page');
    await this.updatePageView(fragment, blob);
    this.pagesElement.appendChild(fragment);
  }

  /**
   * Crops the pages and saves them to the file system as one file. If mimeType
   * is JPEG, only saves the first page.
   */
  private async save(mimeType: MimeType.JPEG|MimeType.PDF): Promise<void> {
    const startTime = performance.now();
    const blobs = this.pages.map((page) => page.croppedBlob);
    const name = (new Filenamer()).newDocumentName(mimeType);
    if (mimeType === MimeType.JPEG) {
      await this.resultSaver.savePhoto(blobs[0], name, null);
    } else {
      const blob = await this.pdfBuilder.save();
      await this.resultSaver.savePhoto(blob, name, null);
    }
    this.lastFileProcessingTime = performance.now() - startTime;
  }

  getLastFileProcessingTime(): number {
    return assertExists(this.lastFileProcessingTime);
  }

  /**
   * Crops the pages and shares them as one file. If mimeType is JPEG, only
   * share the first page.
   */
  private async share(mimeType: MimeType.JPEG|MimeType.PDF): Promise<void> {
    const blobs = this.pages.map((page) => page.croppedBlob);
    const name = (new Filenamer()).newDocumentName(mimeType);
    const blob =
        mimeType === MimeType.JPEG ? blobs[0] : await this.pdfBuilder.save();
    const file = new File([blob], name, {type: mimeType});
    await share(file);
  }

  /**
   * Open DocumentReview view with specific mode. Promise resolves with page
   * count when the view is closed.
   */
  async open({fix}: {fix?: boolean}): Promise<number> {
    assert(
        this.pages.length !== 0,
        'Page count is expected to be be larger than zero');
    await this.selectPage(this.pages.length - 1);
    await this.showMode(fix === true ? Mode.FIX : Mode.PREVIEW);
    await nav.open(this.name).closed;
    return this.pages.length;
  }

  /**
   * Closes DocumentReview view.
   */
  close(): void {
    nav.close(this.name);
  }

  /**
   * Shows/Updates the elements of the mode and hide another mode if necessary.
   */
  async showMode(mode: Mode): Promise<void> {
    if (this.mode !== mode) {
      await this.updateModeView(mode);
      this.modes[this.mode].hide();
      this.modes[mode].show();
      this.mode = mode;
    }
    this.modes[this.mode].focusDefaultElement();
  }

  /**
   * Updates the elements of the mode.
   */
  async updateModeView(mode: Mode): Promise<void> {
    switch (mode) {
      case Mode.FIX: {
        const {blob, corners, rotation} = this.pages[this.selectedIndex];
        await this.modes[mode].update({corners, rotation, blob});
        break;
      }
      case Mode.PREVIEW: {
        const {src} = this.getPageImageElement(
            this.pagesElement.children[this.selectedIndex]);
        await this.modes[mode].update({src, pageIndex: this.selectedIndex});
        break;
      }
      default:
        assertNotReached();
    }
  }

  /**
   * Sets `this.updatingPage` and updates `this.pages` and related elements.
   * Sets `this.pendingUpdatePayload` if there's an ongoing update.
   * `this.pendingUpdatePayload` will be consumed after the current update is
   * done.
   *
   * @return Promise resolves when the update is done and there isn't any
   * pending payload.
   */
  private async updatePage(index: number, page: PageInternal): Promise<void> {
    this.pendingUpdatePayload = [index, page];
    if (this.updatingPage !== null) {
      return;
    }
    while (this.pendingUpdatePayload !== null) {
      this.updatingPage = this.updatePageInternal(...this.pendingUpdatePayload);
      this.pendingUpdatePayload = null;
      await this.updatingPage;
    }
    this.updatingPage = null;
  }

  /**
   * Shows loading view and prevents user actions until the update is done.
   */
  private async waitForUpdatingPage<T>(onUpdated?: () => Promise<T>):
      Promise<T|undefined> {
    if (this.updatingPage === null) {
      return onUpdated?.();
    }
    nav.open(ViewName.FLASH);
    try {
      while (this.updatingPage !== null) {
        await this.updatingPage;
      }
      return await onUpdated?.();
    } finally {
      nav.close(ViewName.FLASH);
    }
  }

  private async updatePageInternal(index: number, page: PageInternal):
      Promise<void> {
    const {blob: croppedBlob} = await this.crop(page);
    const pageElement = this.pagesElement.children[index];
    await this.updatePageView(pageElement, croppedBlob);
    this.pages[index] = {...page, croppedBlob, isDirty: true};
  }

  private async updatePageView(pageElement: ParentNode, blob: Blob):
      Promise<void> {
    const pageImageElement = this.getPageImageElement(pageElement);
    await loadImage(pageImageElement, blob);
  }

  /**
   * The handler called when users delete a page.
   */
  private async onDeletePage(index: number): Promise<void> {
    await this.waitForUpdatingPage();
    sendDocScanEvent(DocScanActionType.DELETE_PAGE);
    await this.deletePage(index);
    speakMessage(getI18nMessage(I18nString.DELETE_PAGE_MESSAGE, index + 1));
    if (this.pages.length === 0) {
      // By design, this line is not reachable. If we decide to let users delete
      // the last page later, we should close the view when no pages remain.
      this.close();
    }
  }

  /**
   * Deletes the page and selects the next page.
   */
  private async deletePage(index: number): Promise<void> {
    this.deletePageView(index);
    this.pages.splice(index, 1);
    this.pdfBuilder.deletePage(index);
    await this.selectPage(
        this.selectedIndex === this.pages.length ? this.pages.length - 1 :
                                                   this.selectedIndex);
  }

  private deletePageView(index: number): void {
    const pageElement = this.pagesElement.children[index];
    const imageElement = this.getPageImageElement(pageElement);
    URL.revokeObjectURL(imageElement.src);
    pageElement.remove();
  }

  // TODO(pihsun): Revisit which operations of document scanning should be on
  // the same queue.
  private async selectPage(index: number) {
    this.selectedIndex = index;
    this.selectPageView(index);
    await this.updateModeView(this.mode);
  }

  /**
   * Changes active page and updates related elements.
   */
  private selectPageView(index: number): void {
    for (const pageElement of this.pagesElement.children) {
      pageElement.classList.remove(ACTIVE_PAGE_CLASS);
      pageElement.setAttribute('aria-selected', 'false');
      pageElement.setAttribute('tabindex', '-1');
    }
    const activePageElement =
        assertInstanceof(this.pagesElement.children[index], HTMLElement);
    activePageElement.classList.add(ACTIVE_PAGE_CLASS);
    activePageElement.setAttribute('aria-selected', 'true');
    activePageElement.setAttribute('tabindex', '0');
    activePageElement.focus();
    activePageElement.scrollIntoView();
  }

  /**
   * Deletes all pages and clears sidebar.
   */
  private clearPages(): void {
    this.pages = [];
    this.pdfBuilder.clear();
    this.clearPagesView();
  }

  private clearPagesView(): void {
    for (const pageElement of this.pagesElement.children) {
      const imageElement = this.getPageImageElement(pageElement);
      URL.revokeObjectURL(imageElement.src);
    }
    this.pagesElement.replaceChildren();
  }

  private async crop(page: Page): Promise<Page> {
    const {blob, corners, rotation} = page;
    const newBlob = await ChromeHelper.getInstance().convertToDocument(
        blob, corners, rotation);
    return {...page, blob: newBlob};
  }

  private getPageImageElement(node: ParentNode) {
    return dom.getFrom(node, '.thumbnail', HTMLImageElement);
  }

  protected override leaving(): boolean {
    // TODO(pihsun): Should have a proper way to "pause" leaving.
    void this.waitForUpdatingPage();
    if (this.pages.length === 0) {
      this.fixCount = 0;
    }
    return true;
  }

  override handlingKey(key: KeyboardShortcut): boolean {
    if (this.pages.length === 1 ||
        !this.pagesElement.contains(document.activeElement)) {
      return false;
    }
    if (key === 'ArrowUp') {
      const index = this.selectedIndex === 0 ? this.pages.length - 1 :
                                               this.selectedIndex - 1;
      // TODO(b/301360817): Revisit which operations should be on the same
      // queue.
      void this.onSelectPage(index);
      return true;
    } else if (key === 'ArrowDown') {
      const index = this.selectedIndex === this.pages.length - 1 ?
          0 :
          this.selectedIndex + 1;
      // TODO(b/301360817): Revisit which operations should be on the same
      // queue.
      void this.onSelectPage(index);
      return true;
    } else if (key === 'Delete') {
      // TODO(b/301360817): Revisit which operations should be on the same
      // queue.
      void this.onDeletePage(this.selectedIndex);
      return true;
    }
    return false;
  }

  protected override setUnfocusable(): void {
    // `tabindex` of page elements might be changed by `selectPage` before the
    // view is opened to avoid flickering. Set `tabindex` to -1 before
    // `super.setUnfocusable()` to avoid `tabindex` being changed to 0 by
    // `super.setFocusable()`.
    if (this.pagesElement.children.length !== 0) {
      const activePageElement = this.pagesElement.children[this.selectedIndex];
      activePageElement.setAttribute('tabindex', '-1');
      activePageElement.setAttribute('aria-selected', 'false');
    }
    super.setUnfocusable();
  }

  private sendResultEvent(action: DocScanResultActionType) {
    const isCornersUpdated = this.pages.some((page) => page.isCornersUpdated);
    const isRotationUpdated = this.pages.some((page) => page.isRotationUpdated);
    let fixType = DocScanFixType.NONE;
    if (isCornersUpdated) {
      fixType |= DocScanFixType.CORNER;
    }
    if (isRotationUpdated) {
      fixType |= DocScanFixType.ROTATION;
    }
    return sendDocScanResultEvent(
        action, fixType, this.fixCount, this.pages.length);
  }

  private updateDeleteButtonLabels() {
    for (let i = 0; i < this.pagesElement.children.length; i++) {
      const deleteButton = dom.getFrom(
          this.pagesElement.children[i], DELETE_PAGE_BUTTON_SELECTOR,
          HTMLElement);
      deleteButton.setAttribute(
          'aria-label', getI18nMessage(I18nString.DELETE_PAGE_BUTTON, i + 1));
    }
  }

  /**
   * The handler called when users select a page.
   */
  private async onSelectPage(index: number) {
    await this.waitForUpdatingPage();
    await this.selectPage(index);
  }
}

/**
 * PdfBuilder allows users to build a PDF progressively. Users should start with
 * a `create()` call and release the PDF with a `clear()` call.
 */
class PdfBuilder {
  private builder: PdfBuilderRemote|null = null;

  /**
   * Creates a PDF. This must be called before any other calls.
   */
  create(): void {
    this.builder = ChromeHelper.getInstance().createPdfBuilder();
  }

  /**
   * Adds a page with the image at `index`. Replace if the page already exists.
   */
  async addPage(jpg: Blob, index: number): Promise<void> {
    assert(this.builder !== null);
    try {
      if (shouldUseBigBuffer()) {
        const bigBuffer = await createBigBufferFromBlob(jpg);
        this.builder.addPage(bigBuffer, index);
        return;
      }
    } catch (e) {
      handleBigBufferError(e);
    }
    const numArray = await createNumArrayFromBlob(jpg);
    this.builder.addPageInline(numArray, index);
  }

  /**
   * Deletes the page at `index`.
   */
  deletePage(index: number): void {
    assert(this.builder !== null);
    this.builder.deletePage(index);
  }

  /**
   * Returns the current PDF.
   */
  async save(): Promise<Blob> {
    assert(this.builder !== null);
    try {
      if (shouldUseBigBuffer()) {
        const {pdf} = await this.builder.save();
        return this.createPdfBlob(pdf);
      }
    } catch (e) {
      handleBigBufferError(e);
    }
    const {pdf} = await this.builder.saveInline();
    return new Blob([new Uint8Array(pdf)], {type: MimeType.PDF});
  }

  /**
   * Releases the resource. Call it when the PDF is no longer needed.
   */
  clear(): void {
    assert(this.builder !== null);
    this.builder.$.close();
    this.builder = null;
  }

  /**
   * Create a PDF Blob from `bigBuffer`.
   *
   * This function handles the different ways the data can be stored in the
   * `bigBuffer` object and returns a Blob containing the PDF data. Only one of
   * the three scenarios will happen:
   *
   * - `invalidBuffer` is true, no data is sent.
   * - `bytes` is defined, creates a Blob from the provided byte array.
   * - `sharedMemory` is defined, maps the shared memory region to a buffer and
   *   creates a Blob from the mapped data.
   */
  private createPdfBlob(bigBuffer: BigBuffer): Blob {
    assert(bigBuffer.invalidBuffer !== true);
    let bytes: Uint8Array|null = null;
    if (bigBuffer.bytes !== undefined) {
      bytes = new Uint8Array(bigBuffer.bytes);
    } else {
      const {bufferHandle, size} = assertExists(bigBuffer.sharedMemory);
      const {result, buffer} = bufferHandle.mapBuffer(0, size);
      assert(result === Mojo.RESULT_OK);
      bytes = new Uint8Array(buffer);
    }
    return new Blob([assertExists(bytes)], {type: MimeType.PDF});
  }
}
