// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './elements/viewer-error-dialog.js';
// <if expr="enable_ink">
import './elements/viewer-ink-host.js';
// </if>
import './elements/viewer-password-dialog.js';
import './elements/viewer-pdf-sidenav.js';
import './elements/viewer-properties-dialog.js';
import './elements/viewer-toolbar.js';
import './elements/shared-vars.css.js';
import './pdf_viewer_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';

import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {listenOnce} from 'chrome://resources/js/util.js';

import {Bookmark} from './bookmark_type.js';
import {BrowserApi} from './browser_api.js';
import {Attachment, DocumentMetadata, ExtendedKeyEvent, FittingType, Point, SaveRequestType} from './constants.js';
import {MessageData, PluginController} from './controller.js';
// <if expr="enable_ink">
import {ContentController} from './controller.js';
// </if>
import {ChangePageAndXyDetail, ChangePageDetail, ChangePageOrigin, NavigateDetail} from './elements/viewer-bookmark.js';
import {ViewerErrorDialogElement} from './elements/viewer-error-dialog.js';
import {ViewerPasswordDialogElement} from './elements/viewer-password-dialog.js';
import {ViewerPdfSidenavElement} from './elements/viewer-pdf-sidenav.js';
import {ViewerToolbarElement} from './elements/viewer-toolbar.js';
// <if expr="enable_ink">
import {InkController, InkControllerEventType} from './ink_controller.js';
//</if>
import {LocalStorageProxyImpl} from './local_storage_proxy.js';
import {record, UserAction} from './metrics.js';
import {NavigatorDelegateImpl, PdfNavigator, WindowOpenDisposition} from './navigator.js';
import {deserializeKeyEvent, LoadState} from './pdf_scripting_api.js';
import {getTemplate} from './pdf_viewer.html.js';
import {KeyEventData, PdfViewerBaseElement} from './pdf_viewer_base.js';
import {DestinationMessageData, DocumentDimensionsMessageData, hasCtrlModifier, hasCtrlModifierOnly, shouldIgnoreKeyEvents} from './pdf_viewer_utils.js';

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
  const mainUrl = url.split(/#|\?/)[0];
  const components = mainUrl.split(/\/|\\/);
  const filename = components[components.length - 1];
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
 * The background color used for the regular viewer. Its decimal value in string
 * format should match `kPdfViewerBackgroundColor` in
 * components/pdf/browser/plugin_response_writer.cc.
 */
const BACKGROUND_COLOR: number = 0xff525659;

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

  static get template() {
    return getTemplate();
  }

  static override get properties() {
    return {
      annotationAvailable_: {
        type: Boolean,
        computed: 'computeAnnotationAvailable_(' +
            'hadPassword_, clockwiseRotations_, canSerializeDocument_,' +
            'twoUpViewEnabled_)',
      },

      annotationMode_: {
        type: Boolean,
        value: false,
      },

      attachments_: {
        type: Array,
        value: () => [],
      },

      bookmarks_: {
        type: Array,
        value: () => [],
      },

      canSerializeDocument_: {
        type: Boolean,
        value: false,
      },

      clockwiseRotations_: {
        type: Number,
        value: 0,
      },

      /** The number of pages in the PDF document. */
      docLength_: Number,

      documentHasFocus_: {
        type: Boolean,
        value: false,
      },

      documentMetadata_: {
        type: Object,
        value: () => {},
      },

      fileName_: String,

      hadPassword_: {
        type: Boolean,
        value: false,
      },

      hasEdits_: {
        type: Boolean,
        value: false,
      },

      hasEnteredAnnotationMode_: {
        type: Boolean,
        value: false,
      },

      isFormFieldFocused_: {
        type: Boolean,
        value: false,
      },

      /** The current loading progress of the PDF document (0 - 100). */
      loadProgress_: Number,

      /** The number of the page being viewed (1-based). */
      pageNo_: Number,

      pdfAnnotationsEnabled_: {
        type: Boolean,
        value: false,
      },

      // <if expr="enable_screen_ai_service">
      pdfOcrEnabled_: {
        type: Boolean,
        value: false,
      },
      // </if>

      printingEnabled_: {
        type: Boolean,
        value: false,
      },

      showPasswordDialog_: {
        type: Boolean,
        value: false,
      },

      showPropertiesDialog_: {
        type: Boolean,
        value: false,
      },

      sidenavCollapsed_: {
        type: Boolean,
        value: false,
      },

      title_: String,

      twoUpViewEnabled_: {
        type: Boolean,
        value: false,
      },

      viewportZoom_: {
        type: Number,
        value: 1,
      },

      zoomBounds_: {
        type: Object,
        value: () => ({min: 0, max: 0}),
      },
    };
  }

  beepCount: number = 0;
  private annotationAvailable_: boolean;
  private annotationMode_: boolean;
  private attachments_: Attachment[];
  private bookmarks_: Bookmark[];
  private canSerializeDocument_: boolean;
  private clockwiseRotations_: number;
  private docLength_: number;
  private documentHasFocus_: boolean;
  private documentMetadata_: DocumentMetadata;
  private embedded_: boolean;
  private fileName_: string;
  private hadPassword_: boolean;
  private hasEdits_: boolean;
  private hasEnteredAnnotationMode_: boolean;
  private isFormFieldFocused_: boolean;
  private loadProgress_: number;
  private navigator_: PdfNavigator|null = null;
  private pageNo_: number;
  private pdfAnnotationsEnabled_: boolean;
  // <if expr="enable_screen_ai_service">
  private pdfOcrEnabled_: boolean;
  // </if>
  private pluginController_: PluginController|null = null;
  private printingEnabled_: boolean;
  private showPasswordDialog_: boolean;
  private showPropertiesDialog_: boolean;
  private sidenavCollapsed_: boolean;

  /**
   * The state to which to restore `sidenavCollapsed_` after exiting annotation
   * mode.
   */
  private sidenavRestoreState_: boolean = false;

  private title_: string;
  private toolbarEnabled_: boolean = false;
  private twoUpViewEnabled_: boolean;
  private viewportZoom_: number;
  private zoomBounds_: ZoomBounds;

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

  getBackgroundColor(): number {
    return BACKGROUND_COLOR;
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
        InkControllerEventType.HAS_UNSAVED_CHANGES,
        () => chrome.mimeHandlerPrivate.setShowBeforeUnloadDialog(true));
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
    if (chrome.mimeHandlerPrivate && chrome.mimeHandlerPrivate.onSave) {
      chrome.mimeHandlerPrivate.onSave.addListener(this.onSave_.bind(this));
    }

    this.embedded_ = this.browserApi!.getStreamInfo().embedded;
  }

  handleKeyEvent(e: KeyboardEvent) {
    if (shouldIgnoreKeyEvents() || e.defaultPrevented) {
      return;
    }

    // Let the viewport handle directional key events.
    if (this.viewport.handleDirectionalKeyEvent(e, this.isFormFieldFocused_)) {
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
    if (e.key === '\\' && e.ctrlKey) {
      this.$.toolbar.fitToggle();
    }
    // TODO: Add handling for additional relevant hotkeys for the new unified
    // toolbar.
  }

  // <if expr="enable_ink">
  private onResetView_() {
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

  /** Handles the annotation mode being toggled on or off. */
  private async onAnnotationModeToggled_(e: CustomEvent<boolean>) {
    const annotationMode = e.detail;
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
  }

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
  // </if>

  private onDisplayAnnotationsChanged_(e: CustomEvent<boolean>) {
    assert(this.currentController);
    this.currentController.setDisplayAnnotations(e.detail);
  }

  private async enterPresentationMode_(): Promise<void> {
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
  }

  private async onPresentClick_() {
    await this.enterPresentationMode_();

    // When fullscreen changes, it means that the user exited Presentation
    // mode.
    await eventToPromise('fullscreenchange', this.$.scroller);

    this.exitPresentationMode_();
  }


  private onPropertiesClick_() {
    assert(!this.showPropertiesDialog_);
    this.showPropertiesDialog_ = true;
  }

  private onPropertiesDialogClose_() {
    assert(this.showPropertiesDialog_);
    this.showPropertiesDialog_ = false;
  }

  /**
   * Changes two up view mode for the controller. Controller will trigger
   * layout update later, which will update the viewport accordingly.
   */
  private onTwoUpViewChanged_(e: CustomEvent<boolean>) {
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

  private onErrorDialog_() {
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

  private onPasswordDialogClose_() {
    this.showPasswordDialog_ = false;
  }

  /**
   * An event handler for handling password-submitted events. These are fired
   * when an event is entered into the password dialog.
   * @param event A password-submitted event.
   */
  private onPasswordSubmitted_(event: CustomEvent<{password: string}>) {
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
    // <if expr="enable_screen_ai_service">
    this.pdfOcrEnabled_ = loadTimeData.getBoolean('pdfOcrEnabled');
    // </if>
    this.printingEnabled_ = loadTimeData.getBoolean('printingEnabled');
    const presetZoomFactors = this.viewport.presetZoomFactors;
    this.zoomBounds_.min = Math.round(presetZoomFactors[0] * 100);
    this.zoomBounds_.max =
        Math.round(presetZoomFactors[presetZoomFactors.length - 1] * 100);
  }

  override handleScriptingMessage(message: MessageEvent<any>) {
    if (super.handleScriptingMessage(message)) {
      return true;
    }

    if (this.delayScriptingMessage(message)) {
      return true;
    }

    switch (message.data.type.toString()) {
      case 'getSelectedText':
        this.pluginController_!.getSelectedText().then(
            this.handleSelectedTextReply.bind(this));
        break;
      case 'print':
        this.pluginController_!.print();
        break;
      case 'selectAll':
        this.pluginController_!.selectAll();
        break;
      default:
        return false;
    }
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
        this.setDocumentDimensions(
            data as unknown as DocumentDimensionsMessageData);
        return;
      case 'email':
        const emailData = data as unknown as EmailMessageData;
        const href = 'mailto:' + emailData.to + '?cc=' + emailData.cc +
            '&bcc=' + emailData.bcc + '&subject=' + emailData.subject +
            '&body=' + emailData.body;
        this.handleNavigate_(href, WindowOpenDisposition.CURRENT_TAB);
        return;
      case 'getPassword':
        this.handlePasswordRequest_();
        return;
      case 'loadProgress':
        const progressData = data as unknown as {progress: number};
        this.updateProgress(progressData.progress);
        return;
      case 'navigate':
        const navigateData = data as unknown as NavigateMessageData;
        this.handleNavigate_(navigateData.url, navigateData.disposition);
        return;
      case 'navigateToDestination':
        const destinationData = data as unknown as DestinationMessageData;
        this.viewport.handleNavigateToDestination(
            destinationData.page, destinationData.x, destinationData.y,
            destinationData.zoom);
        return;
      case 'metadata':
        const metadataData =
            data as unknown as {metadataData: DocumentMetadata};
        this.setDocumentMetadata_(metadataData.metadataData);
        return;
      case 'setIsEditing':
        // Editing mode can only be entered once, and cannot be exited.
        this.hasEdits_ = true;
        return;
      case 'setIsSelecting':
        const selectingData = data as unknown as {isSelecting: boolean};
        this.viewportScroller!.setEnableScrolling(selectingData.isSelecting);
        return;
      case 'setSmoothScrolling':
        this.viewport.setSmoothScrolling(
            (data as unknown as {smoothScrolling: boolean}).smoothScrolling);
        return;
      case 'formFocusChange':
        const focusedData = data as unknown as {focused: boolean};
        this.isFormFieldFocused_ = focusedData.focused;
        return;
      case 'touchSelectionOccurred':
        this.sendScriptingMessage({
          type: 'touchSelectionOccurred',
        });
        return;
      case 'documentFocusChanged':
        const hasFocusData = data as unknown as {hasFocus: boolean};
        this.documentHasFocus_ = hasFocusData.hasFocus;
        return;
      case 'sendKeyEvent':
        const keyEventData = data as unknown as KeyEventData;
        const keyEvent =
            deserializeKeyEvent(keyEventData.keyEvent) as ExtendedKeyEvent;
        keyEvent.fromPlugin = true;
        this.handleKeyEvent(keyEvent);
        return;
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
    document.title = this.title_;
    this.canSerializeDocument_ = this.documentMetadata_.canSerializeDocument;
  }

  /**
   * An event handler for when the browser tells the PDF Viewer to perform a
   * save on the attachment at a certain index. Callers of this function must
   * be responsible for checking whether the attachment size is valid for
   * downloading.
   * @param e The event which contains the index of attachment to be downloaded.
   */
  private async onSaveAttachment_(e: CustomEvent<number>) {
    const index = e.detail;
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
            // Unblock closing the window now that the user has saved
            // successfully.
            chrome.mimeHandlerPrivate.setShowBeforeUnloadDialog(false);
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

  private onToolbarSave_(e: CustomEvent<SaveRequestType>) {
    this.save_(e.detail);
  }

  private onChangePage_(e: CustomEvent<ChangePageDetail>) {
    this.viewport.goToPage(e.detail.page);
    if (e.detail.origin === ChangePageOrigin.BOOKMARK) {
      record(UserAction.FOLLOW_BOOKMARK);
    } else if (e.detail.origin === ChangePageOrigin.PAGE_SELECTOR) {
      record(UserAction.PAGE_SELECTOR_NAVIGATE);
    } else if (e.detail.origin === ChangePageOrigin.THUMBNAIL) {
      record(UserAction.THUMBNAIL_NAVIGATE);
    }
  }

  private onChangePageAndXy_(e: CustomEvent<ChangePageAndXyDetail>) {
    const point = this.viewport.convertPageToScreen(e.detail.page, e.detail);
    this.goToPageAndXy_(e.detail.origin, e.detail.page, point);
  }

  private onNavigate_(e: CustomEvent<NavigateDetail>) {
    const disposition = e.detail.newtab ?
        WindowOpenDisposition.NEW_BACKGROUND_TAB :
        WindowOpenDisposition.CURRENT_TAB;
    this.navigator_!.navigate(e.detail.uri, disposition);
  }

  private onSidenavToggleClick_() {
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
    let result: {fileName: string, dataToSave: ArrayBuffer}|null = null;
    assert(this.currentController);
    if (requestType !== SaveRequestType.ORIGINAL || !this.annotationMode_) {
      result = await this.currentController.save(requestType);
    } else {
      // <if expr="enable_ink">
      // Request type original in annotation mode --> need to exit annotation
      // mode before saving. See https://crbug.com/919364.
      await this.exitAnnotationMode_();
      assert(!this.annotationMode_);
      result = await this.currentController.save(SaveRequestType.ORIGINAL);
      // </if>
    }
    if (result == null) {
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
            // Unblock closing the window now that the user has saved
            // successfully.
            chrome.mimeHandlerPrivate.setShowBeforeUnloadDialog(false);
          });
        });

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

  private async onPrint_() {
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
  private computeAnnotationAvailable_(): boolean {
    return this.canSerializeDocument_ && !this.hadPassword_;
  }

  /** @return Whether the PDF contents are rotated. */
  private isRotated_(): boolean {
    return this.clockwiseRotations_ !== 0;
  }

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
