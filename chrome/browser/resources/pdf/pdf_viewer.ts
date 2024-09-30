// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './elements/viewer_error_dialog.js';
// <if expr="enable_ink">
import './elements/viewer_ink_host.js';
// </if>
import './elements/viewer_password_dialog.js';
// <if expr="enable_pdf_ink2">
import './elements/viewer_side_panel.js';
// </if>
import './elements/viewer_pdf_sidenav.js';
import './elements/viewer_properties_dialog.js';
import './elements/viewer_toolbar.js';

import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {isMac} from 'chrome://resources/js/platform.js';
import {listenOnce} from 'chrome://resources/js/util.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

// <if expr="enable_ink or enable_pdf_ink2">
import {BeforeUnloadProxyImpl} from './before_unload_proxy.js';
// </if>
import type {Bookmark} from './bookmark_type.js';
import type {BrowserApi} from './browser_api.js';
import type {Attachment, DocumentMetadata, ExtendedKeyEvent, Point} from './constants.js';
import {FittingType, FormFieldFocusType, SaveRequestType} from './constants.js';
import type {MessageData} from './controller.js';
import {PluginController} from './controller.js';
// <if expr="enable_pdf_ink2">
import {PluginControllerEventType} from './controller.js';
// </if>
// <if expr="enable_ink">
import type {ContentController} from './controller.js';
// </if>
import type {ChangePageAndXyDetail, ChangePageDetail, NavigateDetail} from './elements/viewer_bookmark.js';
import {ChangePageOrigin} from './elements/viewer_bookmark.js';
import type {ViewerErrorDialogElement} from './elements/viewer_error_dialog.js';
import type {ViewerPasswordDialogElement} from './elements/viewer_password_dialog.js';
// <if expr="enable_ink">
import type {ViewerPdfSidenavElement} from './elements/viewer_pdf_sidenav.js';
//</if>
// <if expr="enable_pdf_ink2">
import type {Ink2ThumbnailData} from './elements/viewer_thumbnail_bar.js';
//</if>
import type {ViewerToolbarElement} from './elements/viewer_toolbar.js';
// <if expr="enable_ink">
import {InkController, InkControllerEventType} from './ink_controller.js';
//</if>
import {LocalStorageProxyImpl} from './local_storage_proxy.js';
import {convertDocumentDimensionsMessage, convertFormFocusChangeMessage, convertLoadProgressMessage} from './message_converter.js';
import {record, recordEnumeration, UserAction} from './metrics.js';
import {NavigatorDelegateImpl, PdfNavigator, WindowOpenDisposition} from './navigator.js';
import {deserializeKeyEvent, LoadState} from './pdf_scripting_api.js';
import {getCss} from './pdf_viewer.css.js';
import {getHtml} from './pdf_viewer.html.js';
import type {KeyEventData} from './pdf_viewer_base.js';
import {PdfViewerBaseElement} from './pdf_viewer_base.js';
import {PdfViewerPrivateProxyImpl} from './pdf_viewer_private_proxy.js';
import type {DocumentDimensionsMessageData} from './pdf_viewer_utils.js';
import {hasCtrlModifier, hasCtrlModifierOnly, shouldIgnoreKeyEvents} from './pdf_viewer_utils.js';

/**
 * Keep in sync with the values for enum PDFPostMessageDataType in
 * tools/metrics/histograms/metadata/pdf/enums.xml.
 * These values are persisted to logs. Entries should not be renumbered, removed
 * or reused.
 */
enum PostMessageDataType {
  GET_SELECTED_TEXT = 0,
  PRINT = 1,
  SELECT_ALL = 2,
}

interface EmailMessageData {
  type: string;
  to: string;
  cc: string;
  bcc: string;
  subject: string;
  body: string;
}

interface NavigateMessageData {
  type: string;
  url: string;
  disposition: WindowOpenDisposition;
}

interface ZoomBounds {
  min: number;
  max: number;
}

/**
 * Return the filename component of a URL, percent decoded if possible.
 * Exported for tests.
 */
export function getFilenameFromURL(url: string): string {
  // Ignore the query and fragment.
  const mainUrl = url.split(/#|\?/)[0] || '';
  const components = mainUrl.split(/\/|\\/);
  const filename = components[components.length - 1] || '';
  try {
    return decodeURIComponent(filename);
  } catch (e) {
    if (e instanceof URIError) {
      return filename;
    }
    throw e;
  }
}

function eventToPromise(event: string, target: HTMLElement): Promise<void> {
  return new Promise(
      resolve => listenOnce(target, event, (_e: Event) => resolve()));
}

const LOCAL_STORAGE_SIDENAV_COLLAPSED_KEY: string = 'sidenavCollapsed';

/**
 * The background color used for the regular viewer.
 */
// LINT.IfChange(PdfBackgroundColor)
const BACKGROUND_COLOR: number = 0xff525659;
const CR23_BACKGROUND_COLOR: number = 0xff282828;
// LINT.ThenChange(//components/pdf/common/pdf_util.cc:PdfBackgroundColor)

export interface PdfViewerElement {
  $: {
    content: HTMLElement,
    scroller: HTMLElement,
    sizer: HTMLElement,
    toolbar: ViewerToolbarElement,
  };
}

export class PdfViewerElement extends PdfViewerBaseElement {
  static get is() {
    return 'pdf-viewer';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      // from PdfViewerBaseElement
      pdfCr23Enabled: {type: Boolean},
      showErrorDialog: {type: Boolean},
      strings: {type: Object},

      annotationMode_: {type: Boolean},
      attachments_: {type: Array},
      bookmarks_: {type: Array},
      canSerializeDocument_: {type: Boolean},
      clockwiseRotations_: {type: Number},

      /** The number of pages in the PDF document. */
      docLength_: {type: Number},
      documentHasFocus_: {type: Boolean},

      documentMetadata_: {type: Object},

      fileName_: {type: String},
      hadPassword_: {type: Boolean},
      hasEdits_: {type: Boolean},
      hasEnteredAnnotationMode_: {type: Boolean},

      // <if expr="enable_pdf_ink2">
      hasInk2Edits_: {type: Boolean},
      // </if>

      formFieldFocus_: {type: String},

      /** The current loading progress of the PDF document (0 - 100). */
      loadProgress_: {type: Number},

      /** The number of the page being viewed (1-based). */
      pageNo_: {type: Number},
      pdfAnnotationsEnabled_: {type: Boolean},

      // <if expr="enable_pdf_ink2">
      pdfInk2Enabled_: {type: Boolean},
      // </if>

      printingEnabled_: {type: Boolean},
      showPasswordDialog_: {type: Boolean},
      showPropertiesDialog_: {type: Boolean},
      sidenavCollapsed_: {type: Boolean},
      title_: {type: String},
      twoUpViewEnabled_: {type: Boolean},
      viewportZoom_: {type: Number},
      zoomBounds_: {type: Object},
    };
  }

  beepCount: number = 0;
  protected annotationMode_: boolean = false;
  protected attachments_: Attachment[] = [];
  protected bookmarks_: Bookmark[] = [];
  private canSerializeDocument_: boolean = false;
  protected clockwiseRotations_: number = 0;
  protected docLength_: number = 0;
  protected documentHasFocus_: boolean = false;
  protected documentMetadata_: DocumentMetadata = {
    author: '',
    canSerializeDocument: false,
    creationDate: '',
    creator: '',
    fileSize: '',
    keywords: '',
    linearized: false,
    modDate: '',
    pageSize: '',
    producer: '',
    subject: '',
    title: '',
    version: '',
  };
  protected embedded_: boolean = false;
  protected fileName_: string = '';
  private hadPassword_: boolean = false;
  protected hasEdits_: boolean = false;
  protected hasEnteredAnnotationMode_: boolean = false;
  // <if expr="enable_pdf_ink2">
  protected hasInk2Edits_: boolean = false;
  private hasSavedEdits_: boolean = false;
  // </if>
  protected formFieldFocus_: FormFieldFocusType = FormFieldFocusType.NONE;
  protected loadProgress_: number = 0;
  private navigator_: PdfNavigator|null = null;
  protected pageNo_: number = 0;
  protected pdfAnnotationsEnabled_: boolean = false;
  // <if expr="enable_pdf_ink2">
  protected pdfInk2Enabled_: boolean = false;
  // </if>
  private pluginController_: PluginController|null = null;
  protected printingEnabled_: boolean = false;
  // <if expr="enable_pdf_ink2">
  private restoreAnnotationMode_: boolean = false;
  // </if>
  // <if expr="enable_ink or enable_pdf_ink2">
  private showBeforeUnloadDialog_: boolean = false;
  // </if>
  protected showPasswordDialog_: boolean = false;
  protected showPropertiesDialog_: boolean = false;
  protected sidenavCollapsed_: boolean;

  // <if expr="enable_ink">
  /**
   * The state to which to restore `sidenavCollapsed_` after exiting annotation
   * mode.
   */
  private sidenavRestoreState_: boolean = false;
  // </if>

  protected title_: string = '';
  protected toolbarEnabled_: boolean = false;
  protected twoUpViewEnabled_: boolean = false;
  protected viewportZoom_: number = 1;
  protected zoomBounds_: ZoomBounds = {min: 0, max: 0};

  // <if expr="enable_ink">
  private inkController_: InkController|null = null;
  // </if>

  constructor() {
    super();

    // TODO(dpapad): Add tests after crbug.com/1111459 is fixed.
    this.sidenavCollapsed_ = Boolean(Number.parseInt(
        LocalStorageProxyImpl.getInstance().getItem(
            LOCAL_STORAGE_SIDENAV_COLLAPSED_KEY)!,
        10));
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('showErrorDialog') && this.showErrorDialog) {
      this.onErrorDialog_();
    }
  }

  getBackgroundColor(): number {
    return this.pdfCr23Enabled ? CR23_BACKGROUND_COLOR : BACKGROUND_COLOR;
  }

  setPluginSrc(plugin: HTMLEmbedElement) {
    plugin.src = this.browserApi!.getStreamInfo().streamUrl;
  }

  init(browserApi: BrowserApi) {
    this.initInternal(
        browserApi, this.$.scroller, this.$.sizer, this.$.content);

    this.pluginController_ = PluginController.getInstance();

    // <if expr="enable_ink">
    this.inkController_ = InkController.getInstance();
    this.inkController_.init(this.viewport);
    this.tracker.add(
        this.inkController_.getEventTarget(),
        InkControllerEventType.HAS_UNSAVED_CHANGES, () => {
          this.setShowBeforeUnloadDialog_(true);
        });
    // </if>

    this.fileName_ = getFilenameFromURL(this.originalUrl);
    this.title_ = this.fileName_;

    assert(this.paramsParser);
    this.toolbarEnabled_ =
        this.paramsParser.shouldShowToolbar(this.originalUrl);
    if (this.toolbarEnabled_) {
      this.$.toolbar.hidden = false;
    }
    const showSidenav = this.paramsParser.shouldShowSidenav(
        this.originalUrl, this.sidenavCollapsed_);
    this.sidenavCollapsed_ = !showSidenav;

    this.navigator_ = new PdfNavigator(
        this.originalUrl, this.viewport, this.paramsParser,
        new NavigatorDelegateImpl(browserApi));

    // Listen for save commands from the browser.
    if (this.pdfOopifEnabled) {
      chrome.pdfViewerPrivate.onSave.addListener(this.onSave_.bind(this));
    } else {
      chrome.mimeHandlerPrivate.onSave.addListener(this.onSave_.bind(this));
    }

    this.embedded_ = this.browserApi!.getStreamInfo().embedded;

    if (this.pdfOopifEnabled && !this.embedded_) {
      // Give the full page PDF viewer focus so it can handle keyboard events
      // immediately.
      window.focus();
    }
  }

  handleKeyEvent(e: KeyboardEvent) {
    if (shouldIgnoreKeyEvents() || e.defaultPrevented) {
      return;
    }

    // Let the viewport handle directional key events.
    if (this.viewport.handleDirectionalKeyEvent(
            e, this.formFieldFocus_ !== FormFieldFocusType.NONE)) {
      return;
    }

    if (document.fullscreenElement !== null) {
      // Disable zoom shortcuts in Presentation mode.
      // Handle '+' and '-' buttons (both in the numpad and elsewhere).
      if (hasCtrlModifier(e) &&
          (e.key === '=' || e.key === '-' || e.key === '+')) {
        e.preventDefault();
      }

      // Disable further key handling when in Presentation mode.
      return;
    }

    switch (e.key) {
      case 'a':
        // Take over Ctrl+A (but not other combinations like Ctrl-Shift-A).
        // Note that on macOS, "Ctrl" is Command.
        if (hasCtrlModifierOnly(e)) {
          this.pluginController_!.selectAll();
          // Since we do selection ourselves.
          e.preventDefault();
        }
        return;
      case '[':
        // Do not use hasCtrlModifier() here, since Command + [ is already
        // taken by the "go back to the previous webpage" action.
        if (e.ctrlKey) {
          this.rotateCounterclockwise();
        }
        return;
      case ']':
        // Do not use hasCtrlModifier() here, since Command + ] is already
        // taken by the "go forward to the next webpage" action.
        if (e.ctrlKey) {
          this.rotateClockwise();
        }
        return;
    }

    // Handle toolbar related key events.
    this.handleToolbarKeyEvent_(e);
  }

  /**
   * Helper for handleKeyEvent dealing with events that control toolbars.
   */
  private handleToolbarKeyEvent_(e: KeyboardEvent) {
    // TODO(thestig): Should this use hasCtrlModifier() or stay as is?
    if (isMac ? !e.metaKey || e.ctrlKey : !e.ctrlKey || e.metaKey) {
      return;
    }

    // TODO: Add handling for additional relevant hotkeys for the new unified
    // toolbar.
    switch (e.key) {
      case '\\':
        this.$.toolbar.fitToggle();
        return;
      // <if expr="enable_pdf_ink2">
      case 'z':
        this.$.toolbar.undo();
        return;
      case 'y':
        this.$.toolbar.redo();
        return;
      // </if>
    }
  }

  // <if expr="enable_ink">
  protected onResetView_() {
    if (this.twoUpViewEnabled_) {
      assert(this.currentController);
      this.currentController.setTwoUpView(false);
    }

    const rotations = this.viewport.getClockwiseRotations();
    switch (rotations) {
      case 0:
        break;
      case 1:
        this.rotateCounterclockwise();
        break;
      case 2:
        this.rotateCounterclockwise();
        this.rotateCounterclockwise();
        break;
      case 3:
        this.rotateClockwise();
        break;
      default:
        assertNotReached('Invalid rotations count: ' + rotations);
    }
  }

  /** @return Resolves when the sidenav animation is complete. */
  private waitForSidenavTransition_(): Promise<void> {
    return eventToPromise(
        'transitionend',
        this.shadowRoot!.querySelector<ViewerPdfSidenavElement>(
            '#sidenav-container')!);
  }

  /**
   * @return Resolves when the sidenav is restored to `sidenavRestoreState_`,
   *     after having been closed for annotation mode.
   */
  private restoreSidenav_(): Promise<void> {
    this.sidenavCollapsed_ = this.sidenavRestoreState_;
    return this.sidenavCollapsed_ ? Promise.resolve() :
                                    this.waitForSidenavTransition_();
  }
  // </if>

  // <if expr="enable_ink or enable_pdf_ink2">
  /** Handles the annotation mode being toggled on or off. */
  protected async onAnnotationModeToggled_(e: CustomEvent<boolean>) {
    const annotationMode = e.detail;
    // <if expr="enable_pdf_ink2">
    if (this.pdfInk2Enabled_) {
      if (!this.restoreAnnotationMode_) {
        record(
            annotationMode ? UserAction.ENTER_ANNOTATION_MODE :
                             UserAction.EXIT_ANNOTATION_MODE);
      }
      this.pluginController_!.setAnnotationMode(annotationMode);
      this.annotationMode_ = annotationMode;
      return;
    }
    // </if> enable_pdf_ink2

    // <if expr="enable_ink">
    if (annotationMode) {
      // Enter annotation mode.
      assert(this.pluginController_!.isActive);
      assert(!this.inkController_!.isActive);
      // TODO(dstockwell): set plugin read-only, begin transition
      this.updateProgress(0);

      this.sidenavRestoreState_ = this.sidenavCollapsed_;
      this.sidenavCollapsed_ = true;
      if (!this.sidenavRestoreState_) {
        // Wait for the animation before proceeding.
        await this.waitForSidenavTransition_();
      }

      // TODO(dstockwell): handle save failure
      const result =
          await this.pluginController_!.save(SaveRequestType.ANNOTATION);
      // Data always exists when save is called with requestType = ANNOTATION.
      assert(result);

      record(UserAction.ENTER_ANNOTATION_MODE);
      this.annotationMode_ = true;
      this.hasEnteredAnnotationMode_ = true;
      // TODO(dstockwell): feed real progress data from the Ink component
      this.updateProgress(50);
      await this.inkController_!.load(result.fileName, result.dataToSave);
      this.currentController = this.inkController_!;
      this.pluginController_!.unload();
      this.updateProgress(100);
    } else {
      // Exit annotation mode.
      record(UserAction.EXIT_ANNOTATION_MODE);
      assert(!this.pluginController_!.isActive);
      assert(this.inkController_!.isActive);
      assert(this.currentController === this.inkController_!);
      // TODO(dstockwell): set ink read-only, begin transition
      this.updateProgress(0);
      this.annotationMode_ = false;
      // This runs separately to allow other consumers of `loaded` to queue
      // up after this task.
      this.loaded!.then(() => {
        this.inkController_!.unload();
      });
      // TODO(dstockwell): handle save failure
      const result =
          await this.inkController_!.save(SaveRequestType.ANNOTATION);
      // Data always exists when save is called with requestType = ANNOTATION.
      await this.restoreSidenav_();
      this.currentController = this.pluginController_!;
      await this.pluginController_!.load(result.fileName, result.dataToSave);
    }
    // </if> enable_ink
  }
  // </if> enable_ink or enable_pdf_ink2

  // <if expr="enable_ink">
  /** Exits annotation mode if active. */
  private async exitAnnotationMode_(): Promise<void> {
    if (!this.$.toolbar.annotationMode) {
      return;
    }
    this.$.toolbar.toggleAnnotation();
    this.annotationMode_ = false;
    await this.restoreSidenav_();
    await this.loaded;
  }
  // </if> enable_ink

  protected onDisplayAnnotationsChanged_(e: CustomEvent<boolean>) {
    assert(this.currentController);
    this.currentController.setDisplayAnnotations(e.detail);
  }

  private async enterPresentationMode_(): Promise<void> {
    // <if expr="enable_pdf_ink2">
    // Exit annotation mode if it was enabled.
    if (this.pdfInk2Enabled_ && this.annotationMode_) {
      this.restoreAnnotationMode_ = true;
      this.$.toolbar.toggleAnnotation();
      assert(!this.annotationMode_);
    }
    // </if>

    const scroller = this.$.scroller;

    this.viewport.saveZoomState();

    await Promise.all([
      eventToPromise('fullscreenchange', scroller),
      scroller.requestFullscreen(),
    ]);

    this.forceFit(FittingType.FIT_TO_HEIGHT);

    // Switch viewport's wheel behavior.
    this.viewport.setPresentationMode(true);

    // Set presentation mode, which restricts the content to read only
    // (e.g. disable forms and links).
    this.pluginController_!.setPresentationMode(true);

    // Nothing else to do here. The viewport will be updated as a result
    // of a 'resize' event callback.
  }

  private exitPresentationMode_(): void {
    // Revert back to the normal state when exiting Presentation mode.
    assert(document.fullscreenElement === null);
    this.viewport.setPresentationMode(false);
    this.pluginController_!.setPresentationMode(false);

    // Ensure that directional keys still work after exiting.
    this.shadowRoot!.querySelector('embed')!.focus();

    // Set zoom back to original zoom before presentation mode.
    this.viewport.restoreZoomState();

    // <if expr="enable_pdf_ink2">
    // Enter annotation mode again if it was enabled before entering
    // Presentation mode.
    if (this.restoreAnnotationMode_) {
      this.$.toolbar.toggleAnnotation();
      assert(this.annotationMode_);
      this.restoreAnnotationMode_ = false;
    }
    // </if>
  }

  protected async onPresentClick_() {
    await this.enterPresentationMode_();

    // When fullscreen changes, it means that the user exited Presentation
    // mode.
    await eventToPromise('fullscreenchange', this.$.scroller);

    this.exitPresentationMode_();
  }


  protected onPropertiesClick_() {
    assert(!this.showPropertiesDialog_);
    this.showPropertiesDialog_ = true;
  }

  protected onPropertiesDialogClose_() {
    assert(this.showPropertiesDialog_);
    this.showPropertiesDialog_ = false;
  }

  /**
   * Changes two up view mode for the controller. Controller will trigger
   * layout update later, which will update the viewport accordingly.
   */
  protected onTwoUpViewChanged_(e: CustomEvent<boolean>) {
    const twoUpViewEnabled = e.detail;
    assert(this.currentController);
    this.currentController.setTwoUpView(twoUpViewEnabled);
    record(
        twoUpViewEnabled ? UserAction.TWO_UP_VIEW_ENABLE :
                           UserAction.TWO_UP_VIEW_DISABLE);
  }

  /**
   * Moves the viewport to a point in a page. Called back after a
   * 'transformPagePointReply' is returned from the plugin.
   * @param origin Identifier for the caller for logging purposes.
   * @param page The index of the page to go to. zero-based.
   * @param message Message received from the plugin containing the x and y to
   *     navigate to in screen coordinates.
   */
  private goToPageAndXy_(
      origin: ChangePageOrigin, page: number, message: Point) {
    this.viewport.goToPageAndXy(page, message.x, message.y);
    if (origin === ChangePageOrigin.BOOKMARK) {
      record(UserAction.FOLLOW_BOOKMARK);
    }
  }

  /** @return The bookmarks. Used for testing. */
  get bookmarks(): Bookmark[] {
    return this.bookmarks_;
  }

  /** @return The title. Used for testing. */
  get pdfTitle(): string {
    return this.title_;
  }

  override setLoadState(loadState: LoadState) {
    super.setLoadState(loadState);
    if (loadState === LoadState.FAILED) {
      this.closePasswordDialog_();
    }
  }

  override updateProgress(progress: number) {
    if (this.toolbarEnabled_) {
      this.loadProgress_ = progress;
    }
    super.updateProgress(progress);
  }

  protected onErrorDialog_() {
    // The error screen can only reload from a normal tab.
    if (!chrome.tabs || this.browserApi!.getStreamInfo().tabId === -1) {
      return;
    }

    const errorDialog =
        this.shadowRoot!.querySelector<ViewerErrorDialogElement>(
            '#error-dialog')!;
    errorDialog.reloadFn = () => {
      chrome.tabs.reload(this.browserApi!.getStreamInfo().tabId);
    };
  }

  private closePasswordDialog_() {
    const passwordDialog =
        this.shadowRoot!.querySelector<ViewerPasswordDialogElement>(
            '#password-dialog')!;
    if (passwordDialog) {
      passwordDialog.close();
    }
  }

  protected onPasswordDialogClose_() {
    this.showPasswordDialog_ = false;
  }

  /**
   * An event handler for handling password-submitted events. These are fired
   * when an event is entered into the password dialog.
   * @param event A password-submitted event.
   */
  protected onPasswordSubmitted_(event: CustomEvent<{password: string}>) {
    this.pluginController_!.getPasswordComplete(event.detail.password);
  }

  updateUiForViewportChange() {
    // Update toolbar elements.
    this.clockwiseRotations_ = this.viewport.getClockwiseRotations();
    this.pageNo_ = this.viewport.getMostVisiblePage() + 1;
    this.twoUpViewEnabled_ = this.viewport.twoUpViewEnabled();

    assert(this.currentController);
    this.currentController.viewportChanged();
  }

  override handleStrings(strings: {[key: string]: string}) {
    super.handleStrings(strings);

    this.pdfAnnotationsEnabled_ =
        loadTimeData.getBoolean('pdfAnnotationsEnabled');
    // <if expr="enable_pdf_ink2">
    this.pdfInk2Enabled_ = loadTimeData.getBoolean('pdfInk2Enabled');
    // </if>
    this.printingEnabled_ = loadTimeData.getBoolean('printingEnabled');
    const presetZoomFactors = this.viewport.presetZoomFactors;
    assert(presetZoomFactors.length > 0);
    this.zoomBounds_.min = Math.round(presetZoomFactors[0]! * 100);
    this.zoomBounds_.max =
        Math.round(presetZoomFactors[presetZoomFactors.length - 1]! * 100);
  }

  override handleScriptingMessage(message: MessageEvent<any>) {
    if (super.handleScriptingMessage(message)) {
      return true;
    }

    if (this.delayScriptingMessage(message)) {
      return true;
    }

    let messageType;
    switch (message.data.type.toString()) {
      case 'getSelectedText':
        messageType = PostMessageDataType.GET_SELECTED_TEXT;
        this.pluginController_!.getSelectedText().then(
            this.handleSelectedTextReply.bind(this));
        break;
      case 'print':
        messageType = PostMessageDataType.PRINT;
        this.pluginController_!.print();
        break;
      case 'selectAll':
        messageType = PostMessageDataType.SELECT_ALL;
        this.pluginController_!.selectAll();
        break;
      default:
        return false;
    }

    recordEnumeration(
        'PDF.PostMessageDataType', messageType,
        Object.keys(PostMessageDataType).length);
    return true;
  }

  handlePluginMessage(e: CustomEvent<MessageData>) {
    const data = e.detail;
    switch (data.type.toString()) {
      case 'attachments':
        const attachmentsData =
            data as unknown as {attachmentsData: Attachment[]};
        this.setAttachments_(attachmentsData.attachmentsData);
        return;
      case 'beep':
        this.handleBeep_();
        return;
      case 'bookmarks':
        const bookmarksData = data as unknown as {bookmarksData: Bookmark[]};
        this.setBookmarks_(bookmarksData.bookmarksData);
        return;
      case 'documentDimensions':
        this.setDocumentDimensions(convertDocumentDimensionsMessage(data));
        return;
      case 'documentFocusChanged':
        const hasFocusData = data as unknown as {hasFocus: boolean};
        this.documentHasFocus_ = hasFocusData.hasFocus;
        return;
      case 'email':
        const emailData = data as unknown as EmailMessageData;
        const href = 'mailto:' + emailData.to + '?cc=' + emailData.cc +
            '&bcc=' + emailData.bcc + '&subject=' + emailData.subject +
            '&body=' + emailData.body;
        this.handleNavigate_(href, WindowOpenDisposition.CURRENT_TAB);
        return;
      case 'executedEditCommand':
        const editCommandData = data as unknown as {editCommand: string};
        const editCommand = editCommandData.editCommand;
        switch (editCommand) {
          case 'Cut':
            record(UserAction.CUT);
            return;
          case 'Copy':
            record(UserAction.COPY);
            return;
          case 'Paste':
            record(UserAction.PASTE);
            return;
        }
        assertNotReached(
            'Unknown executedEditCommand data received: ' + editCommand);
      // <if expr="enable_pdf_ink2">
      case 'finishInkStroke':
        this.handleFinishInkStroke_();
        return;
      // </if>
      case 'formFocusChange':
        const focusedData = convertFormFocusChangeMessage(data);
        this.formFieldFocus_ = focusedData.focused;
        return;
      case 'getPassword':
        this.handlePasswordRequest_();
        return;
      case 'loadProgress':
        const progressData = convertLoadProgressMessage(data);
        this.updateProgress(progressData.progress);
        return;
      case 'metadata':
        const metadataData =
            data as unknown as {metadataData: DocumentMetadata};
        this.setDocumentMetadata_(metadataData.metadataData);
        return;
      case 'navigate':
        const navigateData = data as unknown as NavigateMessageData;
        this.handleNavigate_(navigateData.url, navigateData.disposition);
        return;
      case 'sendKeyEvent':
        const keyEventData = data as unknown as KeyEventData;
        const keyEvent =
            deserializeKeyEvent(keyEventData.keyEvent) as ExtendedKeyEvent;
        keyEvent.fromPlugin = true;
        this.handleKeyEvent(keyEvent);
        return;
      case 'setIsEditing':
        // Editing mode can only be entered once, and cannot be exited.
        this.hasEdits_ = true;
        return;
      case 'startedFindInPage':
        record(UserAction.FIND_IN_PAGE);
        return;
      case 'touchSelectionOccurred':
        this.sendScriptingMessage({
          type: 'touchSelectionOccurred',
        });
        return;
        // <if expr="enable_pdf_ink2">
      case 'updateInk2Thumbnail':
        const thumbnailData = data as unknown as Ink2ThumbnailData;
        this.pluginController_!.getEventTarget().dispatchEvent(
            new CustomEvent<Ink2ThumbnailData>(
                PluginControllerEventType.UPDATE_INK_THUMBNAIL,
                {detail: thumbnailData}));
        return;
        // </if>
    }
    assertNotReached('Unknown message type received: ' + data.type);
  }

  forceFit(view: FittingType): void {
    this.$.toolbar.forceFit(view);
  }

  afterZoom(viewportZoom: number): void {
    this.viewportZoom_ = viewportZoom;
  }

  override setDocumentDimensions(documentDimensions:
                                     DocumentDimensionsMessageData): void {
    super.setDocumentDimensions(documentDimensions);

    // If the document dimensions are received, the password was correct and the
    // password dialog can be dismissed.
    this.closePasswordDialog_();

    if (this.toolbarEnabled_) {
      this.docLength_ = this.documentDimensions!.pageDimensions.length;
    }
  }

  /** Handles a beep request from the current controller. */
  private handleBeep_() {
    // Beeps are annoying, so just track count for now.
    this.beepCount += 1;
  }

  /** Handles a password request from the current controller. */
  private handlePasswordRequest_() {
    // Show the password dialog if it is not already shown. Otherwise, respond
    // to an incorrect password.
    if (!this.showPasswordDialog_) {
      this.showPasswordDialog_ = true;
      this.sendScriptingMessage({type: 'passwordPrompted'});
    } else {
      const passwordDialog =
          this.shadowRoot!.querySelector<ViewerPasswordDialogElement>(
              '#password-dialog')!;
      assert(passwordDialog);
      passwordDialog.deny();
    }
  }

  /** Handles a navigation request from the current controller. */
  private handleNavigate_(url: string, disposition: WindowOpenDisposition):
      void {
    this.navigator_!.navigate(url, disposition);
  }

  // <if expr="enable_pdf_ink2">
  /** Handles a new ink stroke in annotation mode. */
  private handleFinishInkStroke_() {
    this.hasInk2Edits_ = true;
    this.pluginController_!.getEventTarget().dispatchEvent(
        new CustomEvent(PluginControllerEventType.FINISH_INK_STROKE));
    this.setShowBeforeUnloadDialog_(true);
  }
  // </if>

  /** Sets the document attachment data. */
  private setAttachments_(attachments: Attachment[]) {
    this.attachments_ = attachments;
  }

  /** Sets the document bookmarks data. */
  private setBookmarks_(bookmarks: Bookmark[]) {
    this.bookmarks_ = bookmarks;
  }

  /** Sets document metadata from the current controller. */
  private setDocumentMetadata_(metadata: DocumentMetadata) {
    this.documentMetadata_ = metadata;
    this.title_ = this.documentMetadata_.title || this.fileName_;

    // Tab title is updated only when document.title is called in a
    // top-level document (`main_frame` of `WebContents`). For OOPIF PDF viewer,
    // the current document is the child of a top-level document, hence using a
    // private API to set the tab title.
    // NOTE: Title should only be set for full-page PDFs.
    if (this.pdfOopifEnabled && !this.embedded_) {
      PdfViewerPrivateProxyImpl.getInstance().setPdfDocumentTitle(this.title_);
    } else {
      document.title = this.title_;
    }

    this.canSerializeDocument_ = this.documentMetadata_.canSerializeDocument;
  }

  /**
   * An event handler for when the browser tells the PDF Viewer to perform a
   * save on the attachment at a certain index. Callers of this function must
   * be responsible for checking whether the attachment size is valid for
   * downloading.
   * @param e The event which contains the index of attachment to be downloaded.
   */
  protected async onSaveAttachment_(e: CustomEvent<number>) {
    const index = e.detail;
    assert(this.attachments_[index] !== undefined);
    const size = this.attachments_[index].size;
    assert(size !== -1);

    let dataArray: ArrayBuffer[] = [];
    // If the attachment size is 0, skip requesting the backend to fetch the
    // attachment data.
    if (size !== 0) {
      assert(this.currentController);
      const result = await this.currentController.saveAttachment(index);

      // Cap the PDF attachment size at 100 MB. This cap should be kept in sync
      // with and is also enforced in pdf/pdf_view_web_plugin.h.
      const MAX_FILE_SIZE = 100 * 1000 * 1000;
      const bufView = new Uint8Array(result.dataToSave);
      assert(
          bufView.length <= MAX_FILE_SIZE,
          `File too large to be saved: ${bufView.length} bytes.`);
      assert(
          bufView.length === size,
          `Received attachment size does not match its expected value: ${
              size} bytes.`);

      dataArray = [result.dataToSave];
    }

    const blob = new Blob(dataArray);
    const fileName = this.attachments_[index].name;
    chrome.fileSystem.chooseEntry(
        {type: 'saveFile', suggestedName: fileName},
        (entry?: FileSystemFileEntry) => {
          if (chrome.runtime.lastError) {
            if (chrome.runtime.lastError.message !== 'User cancelled') {
              console.error(
                  'chrome.fileSystem.chooseEntry failed: ' +
                  chrome.runtime.lastError.message);
            }
            return;
          }
          entry!.createWriter((writer: FileWriter) => {
            writer.write(blob);
            // <if expr="enable_ink">
            // Unblock closing the window now that the user has saved
            // successfully.
            this.setShowBeforeUnloadDialog_(false);
            // </if>
          });
        });
  }

  /**
   * An event handler for when the browser tells the PDF Viewer to perform a
   * save.
   * @param streamUrl Unique identifier for a PDF Viewer instance.
   */
  private async onSave_(streamUrl: string) {
    if (streamUrl !== this.browserApi!.getStreamInfo().streamUrl) {
      return;
    }

    let saveMode;
    if (this.hasEnteredAnnotationMode_) {
      saveMode = SaveRequestType.ANNOTATION;
    } else if (this.hasEdits_) {
      saveMode = SaveRequestType.EDITED;
    } else {
      saveMode = SaveRequestType.ORIGINAL;
    }

    this.save_(saveMode);
  }

  protected onToolbarSave_(e: CustomEvent<SaveRequestType>) {
    this.save_(e.detail);
  }

  protected onChangePage_(e: CustomEvent<ChangePageDetail>) {
    this.viewport.goToPage(e.detail.page);
    if (e.detail.origin === ChangePageOrigin.BOOKMARK) {
      record(UserAction.FOLLOW_BOOKMARK);
    } else if (e.detail.origin === ChangePageOrigin.PAGE_SELECTOR) {
      record(UserAction.PAGE_SELECTOR_NAVIGATE);
    } else if (e.detail.origin === ChangePageOrigin.THUMBNAIL) {
      record(UserAction.THUMBNAIL_NAVIGATE);
    }
  }

  protected onChangePageAndXy_(e: CustomEvent<ChangePageAndXyDetail>) {
    const point = this.viewport.convertPageToScreen(e.detail.page, e.detail);
    this.goToPageAndXy_(e.detail.origin, e.detail.page, point);
  }

  protected onNavigate_(e: CustomEvent<NavigateDetail>) {
    const disposition = e.detail.newtab ?
        WindowOpenDisposition.NEW_BACKGROUND_TAB :
        WindowOpenDisposition.CURRENT_TAB;
    this.navigator_!.navigate(e.detail.uri, disposition);
  }

  protected onSidenavToggleClick_() {
    this.sidenavCollapsed_ = !this.sidenavCollapsed_;

    // Workaround for crbug.com/1119944, so that the PDF plugin resizes only
    // once when the sidenav is opened/closed.
    const container = this.shadowRoot!.querySelector('#sidenav-container')!;
    if (!this.sidenavCollapsed_) {
      container.classList.add('floating');
      container.addEventListener('transitionend', () => {
        container.classList.remove('floating');
      }, {once: true});
    }

    LocalStorageProxyImpl.getInstance().setItem(
        LOCAL_STORAGE_SIDENAV_COLLAPSED_KEY,
        (this.sidenavCollapsed_ ? 1 : 0).toString());
  }

  // <if expr="enable_pdf_ink2">
  protected onStrokesUpdated_(e: CustomEvent<number>) {
    this.hasInk2Edits_ = e.detail > 0;

    // If the user already saved, always show the beforeunload dialog if the
    // strokes have updated. If the user hasn't saved, only show the
    // beforeunload dialog if there's edits.
    this.setShowBeforeUnloadDialog_(this.hasSavedEdits_ || this.hasInk2Edits_);
  }
  // </if>

  /**
   * Saves the current PDF document to disk.
   */
  private async save_(requestType: SaveRequestType) {
    this.recordSaveMetrics_(requestType);

    // If we have entered annotation mode we must require the local
    // contents to ensure annotations are saved, unless the user specifically
    // requested the original document. Otherwise we would save the cached
    // remote copy without annotations.
    //
    // Always send requests of type ORIGINAL to the plugin controller, not the
    // ink controller. The ink controller always saves the edited document.
    // TODO(dstockwell): Report an error to user if this fails.
    assert(this.currentController);

    // <if expr="enable_ink">
    // For Ink, request type original in annotation mode --> need to exit
    // annotation mode before saving. See https://crbug.com/919364.
    let shouldExitAnnotationMode =
        this.annotationMode_ && requestType === SaveRequestType.ORIGINAL;

    // Ink2 overrides Ink, and Ink2 does not need to exit annotation mode.
    // Only exit annotation mode if Ink2 is disabled.
    // <if expr="enable_pdf_ink2">
    shouldExitAnnotationMode =
        shouldExitAnnotationMode && !this.pdfInk2Enabled_;
    // </if> enable_pdf_ink2

    if (shouldExitAnnotationMode) {
      await this.exitAnnotationMode_();
      assert(!this.annotationMode_);
    }
    // </if> enable_ink

    const result = await this.currentController.save(requestType);
    if (result === null) {
      // The content controller handled the save internally.
      return;
    }

    // Make sure file extension is .pdf, avoids dangerous extensions.
    let fileName = result!.fileName;
    if (!fileName.toLowerCase().endsWith('.pdf')) {
      fileName = fileName + '.pdf';
    }

    // Create blob before callback to avoid race condition.
    const blob = new Blob([result.dataToSave], {type: 'application/pdf'});
    chrome.fileSystem.chooseEntry(
        {
          type: 'saveFile',
          accepts: [{description: '*.pdf', extensions: ['pdf']}],
          suggestedName: fileName,
        },
        (entry?: FileSystemFileEntry) => {
          if (chrome.runtime.lastError) {
            if (chrome.runtime.lastError.message !== 'User cancelled') {
              console.error(
                  'chrome.fileSystem.chooseEntry failed: ' +
                  chrome.runtime.lastError.message);
            }
            return;
          }
          entry!.createWriter((writer: FileWriter) => {
            writer.write(blob);
            // <if expr="enable_ink or enable_pdf_ink2">
            // Unblock closing the window now that the user has saved
            // successfully.
            this.setShowBeforeUnloadDialog_(false);
            // </if>
            // <if expr="enable_pdf_ink2">
            this.hasSavedEdits_ =
                this.hasSavedEdits_ || requestType === SaveRequestType.EDITED;
            // </if>
          });
        });

    // <if expr="enable_pdf_ink2">
    // Ink2 doesn't need to exit annotation mode after save.
    if (this.pdfInk2Enabled_) {
      return;
    }
    // </if>

    // <if expr="enable_ink">
    // Saving in Annotation mode is destructive: crbug.com/919364
    this.exitAnnotationMode_();
    // </if>
  }

  /**
   * Records metrics for saving PDFs.
   */
  private recordSaveMetrics_(requestType: SaveRequestType) {
    record(UserAction.SAVE);
    switch (requestType) {
      case SaveRequestType.ANNOTATION:
        record(UserAction.SAVE_WITH_ANNOTATION);
        break;
      case SaveRequestType.ORIGINAL:
        record(
            this.hasEdits_ ? UserAction.SAVE_ORIGINAL :
                             UserAction.SAVE_ORIGINAL_ONLY);
        break;
      case SaveRequestType.EDITED:
        record(UserAction.SAVE_EDITED);
        break;
    }
  }

  protected async onPrint_() {
    record(UserAction.PRINT);
    // <if expr="enable_ink">
    await this.exitAnnotationMode_();
    // </if>
    assert(this.currentController);
    this.currentController.print();
  }

  /**
   * Updates the toolbar's annotation available flag depending on current
   * conditions.
   * @return Whether annotations are available.
   */
  protected annotationAvailable_(): boolean {
    return this.canSerializeDocument_ && !this.hadPassword_;
  }

  /** @return Whether the PDF contents are rotated. */
  protected isRotated_(): boolean {
    return this.clockwiseRotations_ !== 0;
  }

  // <if expr="enable_pdf_ink2">
  /**
   * @returns Whether the PDF viewer has Ink2 enabled and is in annotation mode.
   */
  protected inInk2AnnotationMode_() {
    return this.pdfInk2Enabled_ && this.annotationMode_;
  }
  // </if>

  // <if expr="enable_ink or enable_pdf_ink2">
  /**
   * Handles the `BeforeUnloadEvent` event.
   * @param event The `BeforeUnloadEvent` object representing the event.
   */
  override onBeforeUnload(event: BeforeUnloadEvent) {
    super.onBeforeUnload(event);
    // When a user tries to leave PDF with unsaved changes, show the 'Leave
    // site' dialog. OOPIF PDF only, since MimeHandler handles the beforeunload
    // event instead.
    if (this.pdfOopifEnabled && this.showBeforeUnloadDialog_) {
      BeforeUnloadProxyImpl.getInstance().preventDefault(event);
    }
  }

  /**
   * Sets whether to show the beforeunload dialog when navigating away from pdf.
   * @param showDialog A boolean indicating whether to show the beforeunload
   * dialog.
   */
  private setShowBeforeUnloadDialog_(showDialog: boolean) {
    if (this.showBeforeUnloadDialog_ === showDialog) {
      return;
    }

    this.showBeforeUnloadDialog_ = showDialog;
    if (!this.pdfOopifEnabled) {
      chrome.mimeHandlerPrivate.setShowBeforeUnloadDialog(showDialog);
    }
  }
  // </if>

  // <if expr="enable_ink">
  getCurrentControllerForTesting(): ContentController|null {
    return this.currentController;
  }
  // </if>
}

declare global {
  interface HTMLElementTagNameMap {
    'pdf-viewer': PdfViewerElement;
  }
}

customElements.define(PdfViewerElement.is, PdfViewerElement);
