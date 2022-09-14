// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertInstanceof, assertNotReached} from '../assert.js';
import {IndicatorType, showIndicator} from '../custom_effect.js';
import * as dom from '../dom.js';
import {Point} from '../geometry.js';
import {I18nString} from '../i18n_string.js';
import {Filenamer} from '../models/file_namer.js';
import {
  getBool as getLocalStorage,
  set as setLocalStorage,
} from '../models/local_storage.js';
import {ResultSaver} from '../models/result_saver.js';
import {ChromeHelper} from '../mojo/chrome_helper.js';
import * as nav from '../nav.js';
import {show as showToast} from '../toast.js';
import {
  LocalStorageKey,
  MimeType,
  Resolution,
  Rotation,
  ViewName,
} from '../type.js';
import {instantiateTemplate, loadImage, share} from '../util.js';

import {DocumentFixMode} from './document_fix_mode.js';
import {DocumentPreviewMode} from './document_preview_mode.js';
import {View} from './view.js';

export interface Page {
  blob: Blob;
  corners: Point[];
  rotation: Rotation;
  resolution: Resolution;
}

export enum Mode {
  FIX,
  PREVIEW,
}

/**
 * View controller for reviewing multi-page document scanning.
 */
export class DocumentReview extends View {
  /**
   * Information of each pages for creating PDF files and sending metrics.
   */
  private pages: Page[] = [];

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

  private readonly classes = {
    active: 'active',
    delete: 'delete',
    page: 'page',
    pages: 'document-pages',
    preview: 'document-preview',
    thumbnail: 'thumbnail',
    single: 'single',
  } as const;

  private readonly pageTemplateSelector = '#document-review-page';

  private readonly modes: {
    [Mode.FIX]: DocumentFixMode,
    [Mode.PREVIEW]: DocumentPreviewMode,
  }

  /**
   * The promise of current page updating process. Null if no pages are being
   * updated.
   */
  private updatingPage: Promise<void>|null = null;

  /**
   * Pending payload for updating pages. Null if there isn't any pending
   * payload.
   */
  private pendingUpdatePayload: [number, Page]|null = null;

  /**
   * The function to hide the multi-page available indicator at leave. Should be
   * set once the indicator shows.
   */
  private hideMultiPageAvailableIndicator: (() => void)|null = null;

  constructor(protected readonly resultSaver: ResultSaver) {
    super(
        ViewName.DOCUMENT_REVIEW,
        {dismissByEsc: true, defaultFocusSelector: 'primary'});
    this.pagesElement =
        dom.getFrom(this.root, `.${this.classes.pages}`, HTMLDivElement);
    this.previewElement =
        dom.getFrom(this.root, `.${this.classes.preview}`, HTMLDivElement);
    this.pagesElement.addEventListener('keydown', (e) => {
      if (e.key === ' ') {
        const target = assertInstanceof(e.target, HTMLElement);
        target.click();
      }
    });
    this.pagesElement.addEventListener('click', async (e: MouseEvent) => {
      const target = assertInstanceof(e.target, HTMLElement);
      const pageElement = target.closest(`.${this.classes.page}`);
      if (pageElement === null) {
        return;
      }
      const index = Array.from(this.pagesElement.children).indexOf(pageElement);
      await this.waitForUpdatingPage();
      const clickOnDeleteButton =
          target.closest(`.${this.classes.delete}`) !== null;
      if (clickOnDeleteButton) {
        await this.deletePage(index);
        if (this.pages.length === 0) {
          this.close();
        }
        return;
      }
      this.selectPage(index);
    });

    const fixMode = new DocumentFixMode({
      target: this.previewElement,
      onExit: () => {
        this.waitForUpdatingPage(() => this.showMode(Mode.PREVIEW));
      },
      onUpdatePage: ({corners, rotation}) => {
        this.updatePage(this.selectedIndex, {
          ...this.pages[this.selectedIndex],
          corners,
          rotation,
        });
      },
    });
    const previewMode = new DocumentPreviewMode({
      target: this.previewElement,
      onAdd: () => {
        this.close();
      },
      onCancel: () => {
        this.clearPages();
        this.close();
      },
      onFix: () => {
        this.showMode(Mode.FIX);
      },
      onShare: () => {
        this.share(
            this.pages.length > 1 ? MimeType.PDF : MimeType.JPEG,
        );
      },
      onSave: (mimeType: MimeType.JPEG|MimeType.PDF) => {
        nav.open(ViewName.FLASH);
        this.save(mimeType).then(() => this.clearPages()).finally(() => {
          this.close();
          nav.close(ViewName.FLASH);
        });
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
    const croppedPage = await this.crop(page);
    await this.addPageView(croppedPage.blob);
    this.pages.push(page);
    this.root.classList.toggle(this.classes.single, this.pages.length === 1);
  }

  private async addPageView(blob: Blob): Promise<void> {
    const fragment = instantiateTemplate(this.pageTemplateSelector);
    await this.updatePageView(fragment, blob);
    this.pagesElement.appendChild(fragment);
  }

  /**
   * Crops the pages and saves them to the file system as one file. If mimeType
   * is JPEG, only saves the first page.
   */
  private async save(mimeType: MimeType.JPEG|MimeType.PDF): Promise<void> {
    const blobs = await Promise.all(this.pages.map(async (page) => {
      const croppedPage = await this.crop(page);
      return croppedPage.blob;
    }));
    const name = (new Filenamer()).newDocumentName(mimeType);
    try {
      if (mimeType === MimeType.JPEG) {
        await this.resultSaver.savePhoto(blobs[0], name, null);
      } else {
        const pdfBlob = await ChromeHelper.getInstance().convertToPdf(blobs);
        await this.resultSaver.savePhoto(pdfBlob, name, null);
      }
    } catch (e) {
      showToast(I18nString.ERROR_MSG_SAVE_FILE_FAILED);
      throw e;
    }
  }

  /**
   * Crops the pages and shares them as one file. If mimeType is JPEG, only
   * share the first page.
   */
  private async share(mimeType: MimeType.JPEG|MimeType.PDF): Promise<void> {
    const blobs = await Promise.all(this.pages.map(async (page) => {
      const croppedPage = await this.crop(page);
      return croppedPage.blob;
    }));
    const name = (new Filenamer()).newDocumentName(mimeType);
    const blob = mimeType === MimeType.JPEG ?
        blobs[0] :
        await ChromeHelper.getInstance().convertToPdf(blobs);
    const file = new File([blob], name, {type: mimeType});
    await share(file);
  }

  /**
   * Open DocumentReview view with specific mode. Promise resolves with page
   * count when the view is closed.
   */
  async open({fix}: {fix?: boolean}): Promise<number> {
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
    if (this.mode === Mode.PREVIEW) {
      this.showMultiPageAvailableIndicatorAtFirstTime();
    }
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
        this.modes[mode].update({src});
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
  private async updatePage(index: number, page: Page): Promise<void> {
    this.pendingUpdatePayload = [index, page];
    if (this.updatingPage !== null) {
      return;
    }
    while (this.pendingUpdatePayload) {
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
    if (!this.updatingPage) {
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

  private async updatePageInternal(index: number, page: Page): Promise<void> {
    const croppedPage = await this.crop(page);
    const pageElement = this.pagesElement.children[index];
    await this.updatePageView(pageElement, croppedPage.blob);
    this.pages[index] = page;
  }

  private async updatePageView(pageElement: ParentNode, blob: Blob):
      Promise<void> {
    const pageImageElement = this.getPageImageElement(pageElement);
    await loadImage(pageImageElement, blob);
  }

  /**
   * Deletes the page and selects the next page.
   */
  private async deletePage(index: number): Promise<void> {
    this.deletePageView(index);
    this.pages.splice(index, 1);
    await this.selectPage(
        this.selectedIndex === this.pages.length ? this.pages.length - 1 :
                                                   this.selectedIndex);
    this.root.classList.toggle(this.classes.single, this.pages.length === 1);
  }

  private deletePageView(index: number): void {
    const pageElement = this.pagesElement.children[index];
    const imageElement = this.getPageImageElement(pageElement);
    URL.revokeObjectURL(imageElement.src);
    pageElement.remove();
  }

  private async selectPage(index: number): Promise<void> {
    this.selectedIndex = index;
    await this.updateModeView(this.mode);
    this.selectPageView(index);
  }

  /**
   * Changes active page and updates related elements.
   */
  private selectPageView(index: number): void {
    for (let i = 0; i < this.pagesElement.children.length; i++) {
      this.pagesElement.children[i].classList.remove(this.classes.active);
    }
    const activePageElement = this.pagesElement.children[index];
    activePageElement.classList.add(this.classes.active);
    activePageElement.scrollIntoView();
  }

  /**
   * Deletes all pages and clears sidebar.
   */
  private clearPages(): void {
    this.pages = [];
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
        blob, corners, rotation, MimeType.JPEG);
    return {...page, blob: newBlob};
  }

  private getPageImageElement(node: ParentNode) {
    return dom.getFrom(node, `.${this.classes.thumbnail}`, HTMLImageElement);
  }

  private showMultiPageAvailableIndicatorAtFirstTime() {
    if (getLocalStorage(LocalStorageKey.DOC_MODE_MULTI_PAGE_TOAST_SHOWN)) {
      return;
    }
    setLocalStorage(LocalStorageKey.DOC_MODE_MULTI_PAGE_TOAST_SHOWN, true);
    const addPageButton = dom.getFrom(
        this.root, 'button[i18n-aria=add_new_page_button]', HTMLButtonElement);
    const {hide} = showIndicator(
        addPageButton, IndicatorType.DOC_MODE_MULTI_PAGE_AVAILABLE);
    addPageButton.addEventListener('click', hide, {once: true});
    this.hideMultiPageAvailableIndicator = () => {
      hide();
      addPageButton.removeEventListener('click', hide);
    };
  }

  protected override leaving(): boolean {
    this.hideMultiPageAvailableIndicator?.();
    this.hideMultiPageAvailableIndicator = null;
    return true;
  }
}
