// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './elements/viewer-password-dialog.js';
import './elements/viewer-properties-dialog.js';
import './elements/shared-vars.js';
// <if expr="chromeos">
import './elements/viewer-ink-host.js';
import './elements/viewer-form-warning.js';
// </if>
import './pdf_viewer_shared_style.js';
import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';

import {assert, assertNotReached} from 'chrome://resources/js/assert.m.js';
import {FocusOutlineManager} from 'chrome://resources/js/cr/ui/focus_outline_manager.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {hasKeyModifiers, listenOnce} from 'chrome://resources/js/util.m.js';
import {html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Bookmark} from './bookmark_type.js';
import {BrowserApi} from './browser_api.js';
import {Attachment, DocumentMetadata, FittingType, Point, SaveRequestType} from './constants.js';
import {PluginController} from './controller.js';
import {ViewerErrorScreenElement} from './elements/viewer-error-screen.js';
import {ViewerPdfSidenavElement} from './elements/viewer-pdf-sidenav.js';
import {ViewerPdfToolbarNewElement} from './elements/viewer-pdf-toolbar-new.js';
// <if expr="chromeos">
import {InkController, InkControllerEventType} from './ink_controller.js';
//</if>
import {LocalStorageProxyImpl} from './local_storage_proxy.js';
import {record, UserAction} from './metrics.js';
import {NavigatorDelegateImpl, PdfNavigator, WindowOpenDisposition} from './navigator.js';
import {OpenPdfParamsParser} from './open_pdf_params_parser.js';
import {DeserializeKeyEvent, LoadState, SerializeKeyEvent} from './pdf_scripting_api.js';
import {PDFViewerBaseElement} from './pdf_viewer_base.js';
import {DestinationMessageData, DocumentDimensionsMessageData, shouldIgnoreKeyEvents} from './pdf_viewer_utils.js';


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
 *   disposition: !WindowOpenDisposition,
 * }}
 */
let NavigateMessageData;

/**
 * @typedef {{
 *   type: string,
 *   messageId: string,
 *   page: number,
 * }}
 */
let GetThumbnailMessageData;

/**
 * @typedef {{
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

/**
 * @param {string} event
 * @param {!HTMLElement} target
 * @return {!Promise<void>}
 */
function eventToPromise(event, target) {
  return new Promise(resolve => listenOnce(target, event, resolve));
}

/** @type {string} */
const LOCAL_STORAGE_SIDENAV_COLLAPSED_KEY = 'sidenavCollapsed';

/** @polymer */
export class PDFViewerElement extends PDFViewerBaseElement {
  static get is() {
    return 'pdf-viewer';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @private */
      annotationAvailable_: {
        type: Boolean,
        computed: 'computeAnnotationAvailable_(' +
            'hadPassword_, clockwiseRotations_, canSerializeDocument_,' +
            'twoUpViewEnabled_)',
      },

      /** @private */
      annotationMode_: {
        type: Boolean,
        value: false,
      },

      /** @private {!Array<!Attachment>} */
      attachments_: {
        type: Array,
        value: () => [],
      },

      /** @private {!Array<!Bookmark>} */
      bookmarks_: {
        type: Array,
        value: () => [],
      },

      /** @private */
      canSerializeDocument_: {
        type: Boolean,
        value: false,
      },

      /** @private */
      clockwiseRotations_: {
        type: Number,
        value: 0,
      },

      /**
       * The number of pages in the PDF document.
       * @private
       */
      docLength_: Number,

      /** @private */
      documentHasFocus_: {
        type: Boolean,
        value: false,
      },

      /** @private {!DocumentMetadata} */
      documentMetadata_: {
        type: Object,
        value: () => {},
      },

      /** @private */
      documentPropertiesEnabled_: {
        type: Boolean,
        value: false,
      },

      /** @private */
      fileName_: String,

      /** @private */
      hadPassword_: {
        type: Boolean,
        value: false,
      },

      /** @private */
      hasEdits_: {
        type: Boolean,
        value: false,
      },

      /** @private */
      hasEnteredAnnotationMode_: {
        type: Boolean,
        value: false,
      },

      /** @private */
      isFormFieldFocused_: {
        type: Boolean,
        value: false,
      },

      /**
       * The current loading progress of the PDF document (0 - 100).
       * @private
       */
      loadProgress_: Number,

      /**
       * The number of the page being viewed (1-based).
       * @private
       */
      pageNo_: Number,

      /** @private */
      pdfAnnotationsEnabled_: {
        type: Boolean,
        value: false,
      },

      /** @private */
      presentationModeEnabled_: {
        type: Boolean,
        value: false,
      },

      /** @private */
      printingEnabled_: {
        type: Boolean,
        value: false,
      },

      /** @private */
      showPasswordDialog_: {
        type: Boolean,
        value: false,
      },

      /** @private */
      showPropertiesDialog_: {
        type: Boolean,
        value: false,
      },

      /** @private */
      sidenavCollapsed_: {
        type: Boolean,
        value: false,
      },

      /** @private */
      title_: String,

      /** @private */
      twoUpViewEnabled_: {
        type: Boolean,
        value: false,
      },

      /** @private */
      viewportZoom_: {
        type: Number,
        value: 1,
      },

      /** @private {!{ min: number, max: number }} */
      zoomBounds_: {
        type: Object,
        value: () => ({min: 0, max: 0}),
      },
    };
  }

  constructor() {
    super();

    // TODO(dpapad): Add tests after crbug.com/1111459 is fixed.
    this.sidenavCollapsed_ = Boolean(Number.parseInt(
        LocalStorageProxyImpl.getInstance().getItem(
            LOCAL_STORAGE_SIDENAV_COLLAPSED_KEY),
        10));

    // Non-Polymer properties

    /** @type {number} */
    this.beepCount = 0;

    /** @private {boolean} */
    this.toolbarEnabled_ = false;

    /** @private {?PdfNavigator} */
    this.navigator_ = null;

    /**
     * The state to restore sidenavCollapsed_ to after exiting annotation mode.
     * @private {boolean}
     */
    this.sidenavRestoreState_ = false;

    /** @private {?PluginController} */
    this.pluginController_ = null;

    // <if expr="chromeos">
    /** @private {?InkController} */
    this.inkController_ = null;
    // </if>

    FocusOutlineManager.forDocument(document);
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
   * @return {!ViewerPdfToolbarNewElement}
   * @private
   */
  getToolbar_() {
    return /** @type {!ViewerPdfToolbarNewElement} */ (this.$$('#toolbar'));
  }

  /** @override */
  getBackgroundColor() {
    return BACKGROUND_COLOR;
  }

  /** @param {!BrowserApi} browserApi */
  init(browserApi) {
    super.init(browserApi);

    this.pluginController_ = PluginController.getInstance();

    // <if expr="chromeos">
    this.inkController_ = InkController.getInstance();
    this.inkController_.init(
        this.viewport, /** @type {!HTMLDivElement} */ (this.getContent()));
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
      this.getToolbar_().hidden = false;
    }

    // Setup the keyboard event listener.
    document.addEventListener(
        'keydown',
        e => this.handleKeyEvent_(/** @type {!KeyboardEvent} */ (e)));

    this.navigator_ = new PdfNavigator(
        this.originalUrl, this.viewport,
        /** @type {!OpenPdfParamsParser} */ (this.paramsParser),
        new NavigatorDelegateImpl(browserApi));

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
    if (e.key === '\\' && e.ctrlKey) {
      this.getToolbar_().fitToggle();
    }
    // TODO: Add handling for additional relevant hotkeys for the new unified
    // toolbar.
  }

  /**
   * Handle key events. These may come from the user directly or via the
   * scripting API.
   * @param {!KeyboardEvent} e the event to handle.
   * @private
   */
  handleKeyEvent_(e) {
    if (shouldIgnoreKeyEvents() || e.defaultPrevented) {
      return;
    }

    // Let the viewport handle directional key events.
    if (this.viewport.handleDirectionalKeyEvent(e, this.isFormFieldFocused_)) {
      return;
    }

    if (document.fullscreenElement !== null) {
      // Disable zoom shortcuts in Presentation mode.
      let hasModifier = e.ctrlKey;
      // <if expr="is_macosx">
      hasModifier = e.metaKey;
      // </if>
      // Handle '+' and '-' buttons (both in the numpad and elsewhere).
      if (hasModifier && (e.key === '=' || e.key === '-' || e.key === '+')) {
        e.preventDefault();
      }

      // Disable further key handling when in Presentation mode.
      return;
    }

    switch (e.key) {
      case 'a':
        if (e.ctrlKey || e.metaKey) {
          this.pluginController_.selectAll();
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
  /** @private */
  onResetView_() {
    if (this.twoUpViewEnabled_) {
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
        break;
    }
  }

  /**
   * @return {!Promise} Resolves when the sidenav animation is complete.
   * @private
   */
  waitForSidenavTransition_() {
    return eventToPromise(
        'transitionend',
        /** @type {!ViewerPdfSidenavElement} */
        (this.shadowRoot.querySelector('#sidenav-container')));
  }

  /**
   * @return {!Promise} Resolves when the sidenav is restored to
   *     |sidenavRestoreState_|, after having been closed for annotation mode.
   * @private
   */
  restoreSidenav_() {
    this.sidenavCollapsed_ = this.sidenavRestoreState_;
    return this.sidenavCollapsed_ ? Promise.resolve() :
                                    this.waitForSidenavTransition_();
  }

  /**
   * Handles the annotation mode being toggled on or off.
   * @param {!CustomEvent<boolean>} e
   * @private
   */
  async onAnnotationModeToggled_(e) {
    const annotationMode = e.detail;
    if (annotationMode) {
      // Enter annotation mode.
      assert(this.pluginController_.isActive);
      assert(!this.inkController_.isActive);
      // TODO(dstockwell): set plugin read-only, begin transition
      this.updateProgress(0);

      this.sidenavRestoreState_ = this.sidenavCollapsed_;
      this.sidenavCollapsed_ = true;
      if (!this.sidenavRestoreState_) {
        // Wait for the animation before proceeding.
        await this.waitForSidenavTransition_();
      }

      // TODO(dstockwell): handle save failure
      const saveResult =
          await this.pluginController_.save(SaveRequestType.ANNOTATION);
      // Data always exists when save is called with requestType = ANNOTATION.
      const result = /** @type {!RequiredSaveResult} */ (saveResult);

      record(UserAction.ENTER_ANNOTATION_MODE);
      this.annotationMode_ = true;
      this.hasEnteredAnnotationMode_ = true;
      // TODO(dstockwell): feed real progress data from the Ink component
      this.updateProgress(50);
      await this.inkController_.load(result.fileName, result.dataToSave);
      this.currentController = this.inkController_;
      this.pluginController_.unload();
      this.updateProgress(100);
    } else {
      // Exit annotation mode.
      record(UserAction.EXIT_ANNOTATION_MODE);
      assert(!this.pluginController_.isActive);
      assert(this.inkController_.isActive);
      assert(this.currentController === this.inkController_);
      // TODO(dstockwell): set ink read-only, begin transition
      this.updateProgress(0);
      this.annotationMode_ = false;
      // This runs separately to allow other consumers of `loaded` to queue
      // up after this task.
      this.loaded.then(() => {
        this.currentController = this.pluginController_;
        this.inkController_.unload();
      });
      // TODO(dstockwell): handle save failure
      const saveResult =
          await this.inkController_.save(SaveRequestType.ANNOTATION);
      // Data always exists when save is called with requestType = ANNOTATION.
      const result = /** @type {!RequiredSaveResult} */ (saveResult);
      await this.restoreSidenav_();
      await this.pluginController_.load(result.fileName, result.dataToSave);
      // Ensure the plugin gets the initial viewport.
      this.pluginController_.afterZoom();
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
    await this.restoreSidenav_();
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
    if (this.pluginController_.isActive && !this.annotationMode_) {
      this.pluginController_.updateScroll(
          e.target.scrollLeft, e.target.scrollTop);
    }
  }

  /** @private */
  onPresentClick_() {
    assert(this.presentationModeEnabled_);

    const onWheel = e => {
      e.deltaY > 0 ? this.viewport.goToNextPage() :
                     this.viewport.goToPreviousPage();
    };

    const scroller = /** @type {!HTMLElement} */ (
        this.shadowRoot.querySelector('#scroller'));

    Promise
        .all([
          eventToPromise('fullscreenchange', scroller),
          scroller.requestFullscreen(),
        ])
        .then(() => {
          this.forceFit(FittingType.FIT_TO_HEIGHT);

          // Add a 'wheel' listener, only while in Presentation mode.
          scroller.addEventListener('wheel', onWheel);

          // Restrict the content to read only (e.g. disable forms and links).
          this.pluginController_.setReadOnly(true);

          // Revert back to the normal state when exiting Presentation mode.
          eventToPromise('fullscreenchange', scroller).then(() => {
            assert(document.fullscreenElement === null);
            scroller.removeEventListener('wheel', onWheel);
            this.pluginController_.setReadOnly(false);

            // Ensure that directional keys still work after exiting.
            this.shadowRoot.querySelector('embed').focus();
          });

          // Nothing else to do here. The viewport will be updated as a result
          // of a 'resize' event callback.
        });
  }

  /** @private */
  onPropertiesClick_() {
    assert(this.documentPropertiesEnabled_);
    assert(!this.showPropertiesDialog_);
    this.showPropertiesDialog_ = true;
  }

  /** @private */
  onPropertiesDialogClose_() {
    assert(this.showPropertiesDialog_);
    this.showPropertiesDialog_ = false;
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
    record(
        twoUpViewEnabled ? UserAction.TWO_UP_VIEW_ENABLE :
                           UserAction.TWO_UP_VIEW_DISABLE);
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
      record(UserAction.FOLLOW_BOOKMARK);
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
      this.closePasswordDialog_();
    }
  }

  /** @override */
  updateProgress(progress) {
    if (this.toolbarEnabled_) {
      this.loadProgress_ = progress;
    }
    super.updateProgress(progress);
  }

  /** @private */
  closePasswordDialog_() {
    const passwordDialog = this.shadowRoot.querySelector('#password-dialog');
    if (passwordDialog) {
      passwordDialog.close();
    }
  }

  /** @private */
  onPasswordDialogClose_() {
    this.showPasswordDialog_ = false;
  }

  /**
   * An event handler for handling password-submitted events. These are fired
   * when an event is entered into the password dialog.
   * @param {!CustomEvent<{password: string}>} event a password-submitted event.
   * @private
   */
  onPasswordSubmitted_(event) {
    this.pluginController_.getPasswordComplete(event.detail.password);
  }

  /** @override */
  updateUIForViewportChange() {
    // Update toolbar elements.
    this.clockwiseRotations_ = this.viewport.getClockwiseRotations();
    this.pageNo_ = this.viewport.getMostVisiblePage() + 1;
    this.twoUpViewEnabled_ = this.viewport.twoUpViewEnabled();

    this.currentController.viewportChanged();
  }

  /** @override */
  handleStrings(strings) {
    super.handleStrings(strings);

    this.documentPropertiesEnabled_ =
        loadTimeData.getBoolean('documentPropertiesEnabled');
    this.pdfAnnotationsEnabled_ =
        loadTimeData.getBoolean('pdfAnnotationsEnabled');
    this.presentationModeEnabled_ =
        loadTimeData.getBoolean('presentationModeEnabled');
    this.printingEnabled_ = loadTimeData.getBoolean('printingEnabled');
    const presetZoomFactors = this.viewport.presetZoomFactors;
    this.zoomBounds_.min = Math.round(presetZoomFactors[0] * 100);
    this.zoomBounds_.max =
        Math.round(presetZoomFactors[presetZoomFactors.length - 1] * 100);
  }

  /** @override */
  handleScriptingMessage(message) {
    super.handleScriptingMessage(message);

    if (this.delayScriptingMessage(message)) {
      return;
    }

    switch (message.data.type.toString()) {
      case 'getSelectedText':
        this.pluginController_.getSelectedText().then(
            this.handleSelectedTextReply.bind(this));
        break;
      case 'getThumbnail':
        const getThumbnailData =
            /** @type {GetThumbnailMessageData} */ (message.data);
        const page = getThumbnailData.page;
        this.pluginController_.requestThumbnail(page).then(
            this.sendScriptingMessage.bind(this));
        break;
      case 'print':
        this.pluginController_.print();
        break;
      case 'selectAll':
        this.pluginController_.selectAll();
        break;
    }
  }

  /** @override */
  handlePluginMessage(e) {
    const data = e.detail;
    switch (data.type.toString()) {
      case 'attachments':
        this.setAttachments_(
            /** @type {{ attachmentsData: !Array<!Attachment> }} */ (data)
                .attachmentsData);
        return;
      case 'beep':
        this.handleBeep_();
        return;
      case 'bookmarks':
        this.setBookmarks_(
            /** @type {{ bookmarksData: !Array<!Bookmark> }} */ (data)
                .bookmarksData);
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
        this.handleNavigate_(href, WindowOpenDisposition.CURRENT_TAB);
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
        this.setDocumentMetadata_(
            /** @type {{ metadataData: !DocumentMetadata }} */ (data)
                .metadataData);
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
    this.getToolbar_().forceFit(view);
  }

  /** @override */
  afterZoom(viewportZoom) {
    this.viewportZoom_ = viewportZoom;
  }

  /** @override */
  setDocumentDimensions(documentDimensions) {
    super.setDocumentDimensions(documentDimensions);

    // If the document dimensions are received, the password was correct and the
    // password dialog can be dismissed.
    this.closePasswordDialog_();

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
    // Show the password dialog if it is not already shown. Otherwise, respond
    // to an incorrect password.
    if (!this.showPasswordDialog_) {
      this.showPasswordDialog_ = true;
      this.sendScriptingMessage({type: 'passwordPrompted'});
    } else {
      const passwordDialog = this.shadowRoot.querySelector('#password-dialog');
      assert(passwordDialog);
      passwordDialog.deny();
    }
  }

  /**
   * Handles a navigation request from the current controller.
   * @param {string} url
   * @param {!WindowOpenDisposition} disposition
   * @private
   */
  handleNavigate_(url, disposition) {
    this.navigator_.navigate(url, disposition);
  }

  /**
   * Sets the document attachment data.
   * @param {!Array<!Attachment>} attachments
   * @private
   */
  setAttachments_(attachments) {
    this.attachments_ = attachments;
  }

  /**
   * Sets the document bookmarks data.
   * @param {!Array<!Bookmark>} bookmarks
   * @private
   */
  setBookmarks_(bookmarks) {
    this.bookmarks_ = bookmarks;
  }

  /**
   * Sets document metadata from the current controller.
   * @param {!DocumentMetadata} metadata
   * @private
   */
  setDocumentMetadata_(metadata) {
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
   * @param {!CustomEvent<number>} e The event which contains the index of
   *     attachment to be downloaded.
   * @private
   */
  async onSaveAttachment_(e) {
    const index = e.detail;
    const size = this.attachments_[index].size;
    assert(size !== -1);

    let dataArray = [];
    // If the attachment size is 0, skip requesting the backend to fetch the
    // attachment data.
    if (size !== 0) {
      const result = await this.currentController.saveAttachment(index);

      // Cap the PDF attachment size at 100 MB. This cap should be kept in sync
      // with and is also enforced in pdf/pdf_view_plugin_base.h.
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
        {type: 'saveFile', suggestedName: fileName}, entry => {
          if (chrome.runtime.lastError) {
            if (chrome.runtime.lastError.message !== 'User cancelled') {
              console.error(
                  'chrome.fileSystem.chooseEntry failed: ' +
                  chrome.runtime.lastError.message);
            }
            return;
          }
          entry.createWriter(writer => {
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
    } else if (this.hasEdits_) {
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
      record(UserAction.FOLLOW_BOOKMARK);
    } else if (e.detail.origin === 'pageselector') {
      record(UserAction.PAGE_SELECTOR_NAVIGATE);
    } else if (e.detail.origin === 'thumbnail') {
      record(UserAction.THUMBNAIL_NAVIGATE);
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
   * @param {!CustomEvent<!{newtab: boolean, uri: string}>} e
   * @private
   */
  onNavigate_(e) {
    const disposition = e.detail.newtab ?
        WindowOpenDisposition.NEW_BACKGROUND_TAB :
        WindowOpenDisposition.CURRENT_TAB;
    this.navigator_.navigate(e.detail.uri, disposition);
  }

  /** @private */
  onSidenavToggleClick_() {
    this.sidenavCollapsed_ = !this.sidenavCollapsed_;

    // Workaround for crbug.com/1119944, so that the PDF plugin resizes only
    // once when the sidenav is opened/closed.
    const container = this.shadowRoot.querySelector('#sidenav-container');
    if (!this.sidenavCollapsed_) {
      container.classList.add('floating');
      container.addEventListener('transitionend', e => {
        container.classList.remove('floating');
      }, {once: true});
    }

    LocalStorageProxyImpl.getInstance().setItem(
        LOCAL_STORAGE_SIDENAV_COLLAPSED_KEY, this.sidenavCollapsed_ ? 1 : 0);
  }

  /**
   * Saves the current PDF document to disk.
   * @param {SaveRequestType} requestType The type of save request.
   * @private
   */
  async save_(requestType) {
    this.recordSaveMetrics_(requestType);

    // If we have entered annotation mode we must require the local
    // contents to ensure annotations are saved, unless the user specifically
    // requested the original document. Otherwise we would save the cached
    // remote copy without annotations.
    //
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
              console.error(
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

  /**
   * Records metrics for saving PDFs.
   * @param {SaveRequestType} requestType The type of save request.
   * @private
   */
  recordSaveMetrics_(requestType) {
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

  /** @private */
  async onPrint_() {
    record(UserAction.PRINT);
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
    return this.canSerializeDocument_ && !this.hadPassword_;
  }

  /**
   * @return {boolean} Whether the PDF contents are rotated.
   * @private
   */
  isRotated_() {
    return this.clockwiseRotations_ !== 0;
  }
}

/**
 * Minimum height for the material toolbar to show (px). Should match the media
 * query in index-material.css. If the window is smaller than this at load,
 * leave no space for the toolbar.
 * @type {number}
 */
const TOOLBAR_WINDOW_MIN_HEIGHT = 250;

/**
 * The background color used for the regular viewer.
 * @type {number}
 */
const BACKGROUND_COLOR = 0xff525659;

customElements.define(PDFViewerElement.is, PDFViewerElement);
