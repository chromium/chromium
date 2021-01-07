// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './elements/viewer-error-screen.js';
import './elements/viewer-password-screen.js';
import './elements/viewer-pdf-sidenav.js';
import './elements/viewer-pdf-toolbar.js';
import './elements/viewer-zoom-toolbar.js';
import './elements/shared-vars.js';
// <if expr="chromeos">
import './elements/viewer-ink-host.js';
import './elements/viewer-form-warning.js';
// </if>
import './pdf_viewer_shared_style.js';
import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';

import {assert, assertNotReached} from 'chrome://resources/js/assert.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {hasKeyModifiers} from 'chrome://resources/js/util.m.js';
import {html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Bookmark} from './bookmark_type.js';
import {BrowserApi} from './browser_api.js';
import {FittingType, Point, SaveRequestType} from './constants.js';
import {ViewerPdfToolbarNewElement} from './elements/viewer-pdf-toolbar-new.js';
// <if expr="chromeos">
import {InkController} from './ink_controller.js';
//</if>
import {LocalStorageProxyImpl} from './local_storage_proxy.js';
import {PDFMetrics} from './metrics.js';
import {NavigatorDelegateImpl, PdfNavigator} from './navigator.js';
import {OpenPdfParamsParser} from './open_pdf_params_parser.js';
import {DeserializeKeyEvent, LoadState, SerializeKeyEvent} from './pdf_scripting_api.js';
import {PDFViewerBaseElement} from './pdf_viewer_base.js';
import {DestinationMessageData, DocumentDimensionsMessageData, shouldIgnoreKeyEvents} from './pdf_viewer_utils.js';
import {ToolbarManager} from './toolbar_manager.js';


/**
 * @typedef {{
 *   type: string,
 *   to: string,
 *   cc: string,
 *   bcc: string,
 *   subject: string,
 *   body: string,
 * }}
 */
let EmailMessageData;

/**
 * @typedef {{
 *   type: string,
 *   url: string,
 *   disposition: !PdfNavigator.WindowOpenDisposition,
 * }}
 */
let NavigateMessageData;

/**
 * @typedef {{
 *   type: string,
 *   title: string,
 *   bookmarks: !Array<!Bookmark>,
 *   canSerializeDocument: boolean,
 * }}
 */
let MetadataMessageData;

/**
 * @typedef {{
 *   hasUnsavedChanges: (boolean|undefined),
 *   fileName: string,
 *   dataToSave: !ArrayBuffer
 * }}
 */
let RequiredSaveResult;

/**
 * Return the filename component of a URL, percent decoded if possible.
 * Exported for tests.
 * @param {string} url The URL to get the filename from.
 * @return {string} The filename component.
 */
export function getFilenameFromURL(url) {
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

/** @type {string} */
const LOCAL_STORAGE_SIDENAV_COLLAPSED_KEY = 'sidenavCollapsed';

export class PDFViewerElement extends PDFViewerBaseElement {
  static get is() {
    return 'pdf-viewer';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      annotationAvailable_: {
        type: Boolean,
        computed: 'computeAnnotationAvailable_(' +
            'hadPassword_, rotated_, canSerializeDocument_)',
      },

      annotationMode_: {
        type: Boolean,
        value: false,
      },

      bookmarks_: Array,

      documentHasFocus_: {
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

      rotated_: Boolean,

      hadPassword_: Boolean,

      canSerializeDocument_: Boolean,

      title_: String,

      isFormFieldFocused_: Boolean,

      pdfViewerUpdateEnabled_: Boolean,

      docLength_: Number,
      // <if expr="chromeos">
      inkController_: Object,
      // </if>
      loadProgress_: Number,
      pageNo_: Number,
      pdfFormSaveEnabled_: Boolean,
      pdfAnnotationsEnabled_: Boolean,
      printingEnabled_: Boolean,
      viewportZoom_: Number,
    };
  }

  constructor() {
    super();

    // Polymer properties
    /** @private {boolean} */
    this.annotationAvailable_;

    /** @private {boolean} */
    this.annotationMode_ = false;

    /** @private {!Array<!Bookmark>} */
    this.bookmarks_ = [];

    /** @private {boolean} */
    this.documentHasFocus_ = false;

    /** @private {boolean} */
    this.hasEdits_ = false;

    /** @private {boolean} */
    this.hasEnteredAnnotationMode_ = false;

    /** @private {boolean} */
    this.rotated_ = false;

    /** @private {boolean} */
    this.hadPassword_ = false;

    /** @private {boolean} */
    this.canSerializeDocument_ = false;

    /** @private {string} */
    this.title_ = '';

    /** @private {boolean} */
    this.isFormFieldFocused_ = false;

    // <if expr="chromeos">
    /** @private {?InkController} */
    this.inkController_ = null;
    // </if>

    /** @private {boolean} */
    this.pdfAnnotationsEnabled_ = false;

    /** @private {boolean} */
    this.pdfFormSaveEnabled_ = false;

    /** @private {boolean} */
    this.printingEnabled_ = false;

    /** @private {number} */
    this.viewportZoom_ = 1;

    // Non-Polymer properties

    /** @type {number} */
    this.beepCount = 0;

    /** @private {boolean} */
    this.hadPassword_ = false;

    /** @private {boolean} */
    this.toolbarEnabled_ = false;

    /** @private {?ToolbarManager} */
    this.toolbarManager_ = null;

    /** @private {?PdfNavigator} */
    this.navigator_ = null;

    /** @private {string} */
    this.title_ = '';

    /**
     * The number of pages in the PDF document.
     * @private {number}
     */
    this.docLength_;

    /**
     * The number of the page being viewed (1-based).
     * @private {number}
     */
    this.pageNo_;

    /**
     * The current loading progress of the PDF document (0 - 100).
     * @private {number}
     */
    this.loadProgress_;

    /** @private {boolean} */
    this.pdfViewerUpdateEnabled_ =
        document.documentElement.hasAttribute('pdf-viewer-update-enabled');

    /** @private {boolean} */
    this.sidenavCollapsed_ = false;

    if (this.pdfViewerUpdateEnabled_) {
      // TODO(dpapad): Add tests after crbug.com/1111459 is fixed.
      this.sidenavCollapsed_ = Boolean(Number.parseInt(
          LocalStorageProxyImpl.getInstance().getItem(
              LOCAL_STORAGE_SIDENAV_COLLAPSED_KEY),
          10));
    }
  }

  /** @override */
  getToolbarHeight() {
    assert(this.paramsParser);
    this.toolbarEnabled_ =
        this.paramsParser.shouldShowToolbar(this.originalUrl);

    // The toolbar does not need to be manually accounted in the
    // PDFViewerUpdate UI.
    if (this.pdfViewerUpdateEnabled_) {
      return 0;
    }

    return this.toolbarEnabled_ ? MATERIAL_TOOLBAR_HEIGHT : 0;
  }

  /** @override */
  hasFixedToolbar() {
    return this.pdfViewerUpdateEnabled_;
  }

  /** @override */
  getContent() {
    return /** @type {!HTMLDivElement} */ (this.$$('#content'));
  }

  /** @override */
  getSizer() {
    return /** @type {!HTMLDivElement} */ (this.$$('#sizer'));
  }

  /** @override */
  getErrorScreen() {
    return /** @type {!ViewerErrorScreenElement} */ (this.$$('#error-screen'));
  }

  /**
   * @return {!ViewerPdfToolbarElement}
   * @private
   */
  getToolbar_() {
    return /** @type {!ViewerPdfToolbarElement} */ (this.$$('#toolbar'));
  }

  /**
   * @return {!ViewerPdfToolbarNewElement}
   * @private
   */
  getToolbarNew_() {
    assert(this.pdfViewerUpdateEnabled_);
    return /** @type {!ViewerPdfToolbarNewElement} */ (this.$$('#toolbar'));
  }

  /**
   * @return {!ViewerZoomToolbarElement}
   * @private
   */
  getZoomToolbar_() {
    return /** @type {!ViewerZoomToolbarElement} */ (this.$$('#zoom-toolbar'));
  }

  /** @override */
  getBackgroundColor() {
    return BACKGROUND_COLOR;
  }

  /** @param {!BrowserApi} browserApi */
  init(browserApi) {
    super.init(browserApi);

    // <if expr="chromeos">
    this.inkController_ = new InkController(
        this.viewport, /** @type {!HTMLDivElement} */ (this.getContent()));
    this.tracker.add(
        this.inkController_.getEventTarget(), 'has-unsaved-changes',
        () => chrome.mimeHandlerPrivate.setShowBeforeUnloadDialog(true));
    // </if>

    this.title_ = getFilenameFromURL(this.originalUrl);
    if (this.toolbarEnabled_) {
      this.getToolbar_().hidden = false;
    }

    if (!this.pdfViewerUpdateEnabled_) {
      this.toolbarManager_ = new ToolbarManager(
          window, this.getToolbar_(), this.getZoomToolbar_());
    }

    // Setup the keyboard event listener.
    document.addEventListener(
        'keydown',
        e => this.handleKeyEvent_(/** @type {!KeyboardEvent} */ (e)));

    const tabId = this.browserApi.getStreamInfo().tabId;
    this.navigator_ = new PdfNavigator(
        this.originalUrl, this.viewport,
        /** @type {!OpenPdfParamsParser} */ (this.paramsParser),
        new NavigatorDelegateImpl(tabId));

    // Listen for save commands from the browser.
    if (chrome.mimeHandlerPrivate && chrome.mimeHandlerPrivate.onSave) {
      chrome.mimeHandlerPrivate.onSave.addListener(url => this.onSave_(url));
    }
  }

  /**
   * Helper for handleKeyEvent_ dealing with events that control toolbars.
   * @param {!KeyboardEvent} e the event to handle.
   * @private
   */
  handleToolbarKeyEvent_(e) {
    if (this.pdfViewerUpdateEnabled_) {
      if (e.key === '\\' && e.ctrlKey) {
        this.getToolbarNew_().fitToggle();
      }
      // TODO: Add handling for additional relevant hotkeys for the new unified
      // toolbar.
      return;
    }

    switch (e.key) {
      case 'Tab':
        this.toolbarManager_.showToolbarsForKeyboardNavigation();
        return;
      case 'Escape':
        this.toolbarManager_.hideSingleToolbarLayer();
        return;
      case 'g':
        if (this.toolbarEnabled_ && (e.ctrlKey || e.metaKey) && e.altKey) {
          this.toolbarManager_.showToolbars();
          this.getToolbar_().selectPageNumber();
        }
        return;
      case '\\':
        if (e.ctrlKey) {
          this.getZoomToolbar_().fitToggleFromHotKey();
        }
        return;
    }

    // Show toolbars as a fallback.
    if (!(e.shiftKey || e.ctrlKey || e.altKey)) {
      this.toolbarManager_.showToolbars();
    }
  }

  /**
   * Handle key events. These may come from the user directly or via the
   * scripting API.
   * @param {!KeyboardEvent} e the event to handle.
   * @private
   */
  handleKeyEvent_(e) {
    if (shouldIgnoreKeyEvents(document.activeElement) || e.defaultPrevented) {
      return;
    }

    if (!this.pdfViewerUpdateEnabled_) {
      this.toolbarManager_.hideToolbarsAfterTimeout();
    }

    // Let the viewport handle directional key events.
    if (this.viewport.handleDirectionalKeyEvent(e, this.isFormFieldFocused_)) {
      return;
    }

    switch (e.key) {
      case 'a':
        if (e.ctrlKey || e.metaKey) {
          this.pluginController.selectAll();
          // Since we do selection ourselves.
          e.preventDefault();
        }
        return;
      case '[':
        if (e.ctrlKey) {
          this.rotateCounterclockwise();
        }
        return;
      case ']':
        if (e.ctrlKey) {
          this.rotateClockwise();
        }
        return;
    }

    // Handle toolbar related key events.
    this.handleToolbarKeyEvent_(e);
  }

  // <if expr="chromeos">
  /**
   * Handles the annotation mode being toggled on or off.
   * @param {!CustomEvent<boolean>} e
   * @private
   */
  async onAnnotationModeToggled_(e) {
    const annotationMode = e.detail;
    this.annotationMode_ = annotationMode;
    if (annotationMode) {
      // Enter annotation mode.
      assert(this.currentController === this.pluginController);
      // TODO(dstockwell): set plugin read-only, begin transition
      this.updateProgress(0);
      // TODO(dstockwell): handle save failure
      const saveResult =
          await this.pluginController.save(SaveRequestType.ANNOTATION);
      // Data always exists when save is called with requestType = ANNOTATION.
      const result = /** @type {!RequiredSaveResult} */ (saveResult);
      if (result.hasUnsavedChanges) {
        assert(!loadTimeData.getBoolean('pdfFormSaveEnabled'));
        try {
          await this.$$('#form-warning').show();
        } catch (e) {
          // The user aborted entering annotation mode. Revert to the plugin.
          this.getToolbar_().annotationMode = false;
          this.annotationMode_ = false;
          this.updateProgress(100);
          return;
        }
      }
      PDFMetrics.record(PDFMetrics.UserAction.ENTER_ANNOTATION_MODE);
      this.hasEnteredAnnotationMode_ = true;
      // TODO(dstockwell): feed real progress data from the Ink component
      this.updateProgress(50);
      await this.inkController_.load(result.fileName, result.dataToSave);
      this.currentController = this.inkController_;
      this.pluginController.unload();
      this.updateProgress(100);
    } else {
      // Exit annotation mode.
      PDFMetrics.record(PDFMetrics.UserAction.EXIT_ANNOTATION_MODE);
      assert(this.currentController === this.inkController_);
      // TODO(dstockwell): set ink read-only, begin transition
      this.updateProgress(0);
      // This runs separately to allow other consumers of `loaded` to queue
      // up after this task.
      this.loaded.then(() => {
        this.currentController = this.pluginController;
        this.inkController_.unload();
      });
      // TODO(dstockwell): handle save failure
      const saveResult =
          await this.inkController_.save(SaveRequestType.ANNOTATION);
      // Data always exists when save is called with requestType = ANNOTATION.
      const result = /** @type {!RequiredSaveResult} */ (saveResult);
      await this.pluginController.load(result.fileName, result.dataToSave);
      // Ensure the plugin gets the initial viewport.
      this.pluginController.afterZoom();
    }
  }

  /**
   * Exits annotation mode if active.
   * @return {Promise<void>}
   */
  async exitAnnotationMode_() {
    if (!this.getToolbar_().annotationMode) {
      return;
    }
    this.getToolbar_().toggleAnnotation();
    this.annotationMode_ = false;
    await this.loaded;
  }
  // </if>

  /**
   * @param {!Event} e
   * @private
   */
  onDisplayAnnotationsChanged_(e) {
    this.currentController.setDisplayAnnotations(e.detail);
  }

  /**
   * @param {!Event} e
   * @private
   */
  onScroll_(e) {
    this.pluginController.updateScroll(e.target.scrollLeft, e.target.scrollTop);
  }

  /** @override */
  onFitToChanged(e) {
    super.onFitToChanged(e);

    if (this.pdfViewerUpdateEnabled_) {
      return;
    }

    if (e.detail === FittingType.FIT_TO_PAGE ||
        e.detail === FittingType.FIT_TO_HEIGHT) {
      this.toolbarManager_.forceHideTopToolbar();
    }
  }

  /**
   * Changes two up view mode for the controller. Controller will trigger
   * layout update later, which will update the viewport accordingly.
   * @param {!CustomEvent<boolean>} e
   * @private
   */
  onTwoUpViewChanged_(e) {
    const twoUpViewEnabled = e.detail;
    this.currentController.setTwoUpView(twoUpViewEnabled);
    if (!this.pdfViewerUpdateEnabled_) {
      this.toolbarManager_.forceHideTopToolbar();
    }
    this.getToolbar_().annotationAvailable = !twoUpViewEnabled;
    PDFMetrics.recordTwoUpViewEnabled(twoUpViewEnabled);
  }

  /**
   * Moves the viewport to a point in a page. Called back after a
   * 'transformPagePointReply' is returned from the plugin.
   * @param {string} origin Identifier for the caller for logging purposes.
   * @param {number} page The index of the page to go to. zero-based.
   * @param {Point} message Message received from the plugin containing the
   *     x and y to navigate to in screen coordinates.
   * @private
   */
  goToPageAndXY_(origin, page, message) {
    this.viewport.goToPageAndXY(page, message.x, message.y);
    if (origin === 'bookmark') {
      PDFMetrics.record(PDFMetrics.UserAction.FOLLOW_BOOKMARK);
    }
  }

  /** @return {!Viewport} The viewport. Used for testing. */
  /** @return {!Array<!Bookmark>} The bookmarks. Used for testing. */
  get bookmarks() {
    return this.bookmarks_;
  }

  /** @override */
  setLoadState(loadState) {
    super.setLoadState(loadState);
    if (loadState === LoadState.FAILED) {
      const passwordScreen = this.$$('#password-screen');
      if (passwordScreen && passwordScreen.active) {
        passwordScreen.deny();
        passwordScreen.close();
      }
    }
  }

  /** @override */
  updateProgress(progress) {
    if (this.toolbarEnabled_) {
      this.loadProgress_ = progress;
    }
    super.updateProgress(progress);
    if (progress === 100 && !this.pdfViewerUpdateEnabled_) {
      this.toolbarManager_.hideToolbarsAfterTimeout();
    }
  }

  /**
   * An event handler for handling password-submitted events. These are fired
   * when an event is entered into the password screen.
   * @param {!CustomEvent<{password: string}>} event a password-submitted event.
   * @private
   */
  onPasswordSubmitted_(event) {
    this.pluginController.getPasswordComplete(event.detail.password);
  }

  /** @override */
  updateUIForViewportChange() {
    if (!this.pdfViewerUpdateEnabled_) {
      this.getZoomToolbar_().shiftForScrollbars(
          this.viewport.documentHasScrollbars(), this.viewport.scrollbarWidth);
    }

    // Update the page indicator.
    if (this.toolbarEnabled_) {
      const visiblePage = this.viewport.getMostVisiblePage();
      this.pageNo_ = visiblePage + 1;
    }

    this.currentController.viewportChanged();
  }

  /** @override */
  handleStrings(strings) {
    super.handleStrings(strings);

    this.pdfAnnotationsEnabled_ =
        loadTimeData.getBoolean('pdfAnnotationsEnabled');
    this.pdfFormSaveEnabled_ = loadTimeData.getBoolean('pdfFormSaveEnabled');
    this.printingEnabled_ = loadTimeData.getBoolean('printingEnabled');
  }

  /** @override */
  handleScriptingMessage(message) {
    super.handleScriptingMessage(message);

    if (this.delayScriptingMessage(message)) {
      return;
    }

    switch (message.data.type.toString()) {
      case 'getSelectedText':
        this.pluginController.getSelectedText().then(
            this.handleSelectedTextReply.bind(this));
        break;
      case 'print':
        this.pluginController.print();
        break;
      case 'selectAll':
        this.pluginController.selectAll();
        break;
    }
  }

  /** @override */
  handlePluginMessage(e) {
    const data = e.detail;
    switch (data.type.toString()) {
      case 'beep':
        this.handleBeep_();
        return;
      case 'documentDimensions':
        this.setDocumentDimensions(
            /** @type {!DocumentDimensionsMessageData} */ (data));
        return;
      case 'email':
        const emailData = /** @type {!EmailMessageData} */ (data);
        const href = 'mailto:' + emailData.to + '?cc=' + emailData.cc +
            '&bcc=' + emailData.bcc + '&subject=' + emailData.subject +
            '&body=' + emailData.body;
        this.handleNavigate_(href, PdfNavigator.WindowOpenDisposition.CURRENT_TAB);
        return;
      case 'getPassword':
        this.handlePasswordRequest_();
        return;
      case 'loadProgress':
        this.updateProgress(
            /** @type {{ progress: number }} */ (data).progress);
        return;
      case 'navigate':
        const navigateData = /** @type {!NavigateMessageData} */ (data);
        this.handleNavigate_(navigateData.url, navigateData.disposition);
        return;
      case 'navigateToDestination':
        const destinationData = /** @type {!DestinationMessageData} */ (data);
        this.viewport.handleNavigateToDestination(
            destinationData.page, destinationData.x, destinationData.y,
            destinationData.zoom);
        return;
      case 'metadata':
        const metadata = /** @type {!MetadataMessageData} */ (data);
        this.setDocumentMetadata_(
            metadata.title, metadata.bookmarks, metadata.canSerializeDocument);
        return;
      case 'setIsEditing':
        // Editing mode can only be entered once, and cannot be exited.
        this.hasEdits_ = true;
        return;
      case 'setIsSelecting':
        this.viewportScroller.setEnableScrolling(
            /** @type {{ isSelecting: boolean }} */ (data).isSelecting);
        return;
      case 'formFocusChange':
        this.isFormFieldFocused_ =
            /** @type {{ focused: boolean }} */ (data).focused;
        return;
      case 'touchSelectionOccurred':
        this.sendScriptingMessage({
          type: 'touchSelectionOccurred',
        });
        return;
      case 'documentFocusChanged':
        this.documentHasFocus_ =
            /** @type {{ hasFocus: boolean }} */ (data).hasFocus;
        return;
    }
    assertNotReached('Unknown message type received: ' + data.type);
  }

  /** @override */
  forceFit(view) {
    if (!this.pdfViewerUpdateEnabled_) {
      if (view === FittingType.FIT_TO_PAGE ||
          view === FittingType.FIT_TO_HEIGHT) {
        this.toolbarManager_.forceHideTopToolbar();
      }
      this.getZoomToolbar_().forceFit(view);
    } else {
      this.getToolbarNew_().forceFit(view);
    }
  }

  /** @override */
  afterZoom(viewportZoom) {
    this.viewportZoom_ = viewportZoom;
  }

  /** @override */
  setDocumentDimensions(documentDimensions) {
    super.setDocumentDimensions(documentDimensions);
    // If we received the document dimensions, the password was good so we
    // can dismiss the password screen.
    const passwordScreen = this.$$('#password-screen');
    if (passwordScreen && passwordScreen.active) {
      passwordScreen.close();
    }

    if (this.toolbarEnabled_) {
      this.docLength_ = this.documentDimensions.pageDimensions.length;
    }
  }

  /**
   * Handles a beep request from the current controller.
   * @private
   */
  handleBeep_() {
    // Beeps are annoying, so just track count for now.
    this.beepCount += 1;
  }

  /**
   * Handles a password request from the current controller.
   * @private
   */
  handlePasswordRequest_() {
    // If the password screen isn't up, put it up. Otherwise we're
    // responding to an incorrect password so deny it.
    const passwordScreen = this.$$('#password-screen');
    assert(passwordScreen);
    if (!passwordScreen.active) {
      this.hadPassword_ = true;
      passwordScreen.show();
    } else {
      passwordScreen.deny();
    }
  }

  /**
   * Handles a navigation request from the current controller.
   * @param {string} url
   * @param {!PdfNavigator.WindowOpenDisposition} disposition
   * @private
   */
  handleNavigate_(url, disposition) {
    this.navigator_.navigate(url, disposition);
  }

  /**
   * Sets document metadata from the current controller.
   * @param {string} title
   * @param {!Array<!Bookmark>} bookmarks
   * @param {boolean} canSerializeDocument
   * @private
   */
  setDocumentMetadata_(title, bookmarks, canSerializeDocument) {
    this.title_ = title ? title : getFilenameFromURL(this.originalUrl);
    document.title = this.title_;
    this.bookmarks_ = bookmarks;
    this.canSerializeDocument_ = canSerializeDocument;
  }

  /**
   * An event handler for when the browser tells the PDF Viewer to perform a
   * save.
   * @param {string} streamUrl unique identifier for a PDF Viewer instance.
   * @private
   */
  async onSave_(streamUrl) {
    if (streamUrl !== this.browserApi.getStreamInfo().streamUrl) {
      return;
    }

    let saveMode;
    if (this.hasEnteredAnnotationMode_) {
      saveMode = SaveRequestType.ANNOTATION;
    } else if (
        loadTimeData.getBoolean('pdfFormSaveEnabled') && this.hasEdits_) {
      saveMode = SaveRequestType.EDITED;
    } else {
      saveMode = SaveRequestType.ORIGINAL;
    }

    this.save_(saveMode);
  }

  /**
   * @param {!CustomEvent<!SaveRequestType>} e
   * @private
   */
  onToolbarSave_(e) {
    this.save_(e.detail);
  }

  /**
   * @param {!CustomEvent<!{page: number, origin: string}>} e
   * @private
   */
  onChangePage_(e) {
    this.viewport.goToPage(e.detail.page);
    if (e.detail.origin === 'bookmark') {
      PDFMetrics.record(PDFMetrics.UserAction.FOLLOW_BOOKMARK);
    } else if (e.detail.origin === 'pageselector') {
      PDFMetrics.record(PDFMetrics.UserAction.PAGE_SELECTOR_NAVIGATE);
    }
  }

  /**
   * @param {!CustomEvent<!{
   *   page: number, origin: string, x: number, y: number}>} e
   * @private
   */
  onChangePageAndXy_(e) {
    const point = this.viewport.convertPageToScreen(e.detail.page, e.detail);
    this.goToPageAndXY_(e.detail.origin, e.detail.page, point);
  }

  /**
   * @param {!CustomEvent<string>} e
   * @private
   */
  onDropdownOpened_(e) {
    if (e.detail === 'bookmarks') {
      PDFMetrics.record(PDFMetrics.UserAction.OPEN_BOOKMARKS_PANEL);
    }
  }

  /**
   * @param {!CustomEvent<!{newtab: boolean, uri: string}>} e
   * @private
   */
  onNavigate_(e) {
    const disposition = e.detail.newtab ?
        PdfNavigator.WindowOpenDisposition.NEW_BACKGROUND_TAB :
        PdfNavigator.WindowOpenDisposition.CURRENT_TAB;
    this.navigator_.navigate(e.detail.uri, disposition);
  }

  /** @private */
  onSidenavToggleClick_() {
    assert(this.pdfViewerUpdateEnabled_);
    this.sidenavCollapsed_ = !this.sidenavCollapsed_;
    LocalStorageProxyImpl.getInstance().setItem(
        LOCAL_STORAGE_SIDENAV_COLLAPSED_KEY, this.sidenavCollapsed_ ? 1 : 0);
  }

  /**
   * Saves the current PDF document to disk.
   * @param {SaveRequestType} requestType The type of save request.
   * @private
   */
  async save_(requestType) {
    PDFMetrics.record(PDFMetrics.UserAction.SAVE);
    // If we have entered annotation mode we must require the local
    // contents to ensure annotations are saved, unless the user specifically
    // requested the original document. Otherwise we would save the cached
    // remote copy without annotations.
    if (requestType === SaveRequestType.ANNOTATION) {
      PDFMetrics.record(PDFMetrics.UserAction.SAVE_WITH_ANNOTATION);
    }
    // Always send requests of type ORIGINAL to the plugin controller, not the
    // ink controller. The ink controller always saves the edited document.
    // TODO(dstockwell): Report an error to user if this fails.
    let result;
    if (requestType !== SaveRequestType.ORIGINAL || !this.annotationMode_) {
      result = await this.currentController.save(requestType);
    } else {
      // <if expr="chromeos">
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
    let fileName = result.fileName;
    if (!fileName.toLowerCase().endsWith('.pdf')) {
      fileName = fileName + '.pdf';
    }

    chrome.fileSystem.chooseEntry(
        {
          type: 'saveFile',
          accepts: [{description: '*.pdf', extensions: ['pdf']}],
          suggestedName: fileName
        },
        entry => {
          if (chrome.runtime.lastError) {
            if (chrome.runtime.lastError.message !== 'User cancelled') {
              console.log(
                  'chrome.fileSystem.chooseEntry failed: ' +
                  chrome.runtime.lastError.message);
            }
            return;
          }
          entry.createWriter(writer => {
            writer.write(
                new Blob([result.dataToSave], {type: 'application/pdf'}));
            // Unblock closing the window now that the user has saved
            // successfully.
            chrome.mimeHandlerPrivate.setShowBeforeUnloadDialog(false);
          });
        });

    // <if expr="chromeos">
    // Saving in Annotation mode is destructive: crbug.com/919364
    this.exitAnnotationMode_();
    // </if>
  }

  /** @private */
  async onPrint_() {
    PDFMetrics.record(PDFMetrics.UserAction.PRINT);
    // <if expr="chromeos">
    await this.exitAnnotationMode_();
    // </if>
    this.currentController.print();
  }

  /**
   * Updates the toolbar's annotation available flag depending on current
   * conditions.
   * @return {boolean} Whether annotations are available.
   * @private
   */
  computeAnnotationAvailable_() {
    return this.canSerializeDocument_ && !this.rotated_ && !this.hadPassword_;
  }

  /** @override */
  rotateClockwise() {
    super.rotateClockwise();
    this.rotated_ = this.viewport.getClockwiseRotations() !== 0;
  }

  /** @override */
  rotateCounterclockwise() {
    super.rotateCounterclockwise();
    this.rotated_ = this.viewport.getClockwiseRotations() !== 0;
  }
}

/**
 * The height of the toolbar along the top of the page. The document will be
 * shifted down by this much in the viewport.
 * @type {number}
 */
const MATERIAL_TOOLBAR_HEIGHT = 56;

/**
 * Minimum height for the material toolbar to show (px). Should match the media
 * query in index-material.css. If the window is smaller than this at load,
 * leave no space for the toolbar.
 * @type {number}
 */
const TOOLBAR_WINDOW_MIN_HEIGHT = 250;

/**
 * The background color used for the regular viewer.
 * @type {string}
 */
const BACKGROUND_COLOR = '0xFF525659';

customElements.define(PDFViewerElement.is, PDFViewerElement);
