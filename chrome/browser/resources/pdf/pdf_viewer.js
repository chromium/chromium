// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertNotReached} from 'chrome://resources/js/assert.m.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';
import {$, hasKeyModifiers, isRTL} from 'chrome://resources/js/util.m.js';

import {BrowserApi} from './browser_api.js';
import {ContentController, InkController, MessageData, PluginController, PrintPreviewParams} from './controller.js';
import {Bookmark} from './elements/viewer-bookmark.js';
import {FitToChangedEvent} from './elements/viewer-zoom-toolbar.js';
import {GestureDetector} from './gesture_detector.js';
import {PDFMetrics} from './metrics.js';
import {NavigatorDelegate, PdfNavigator} from './navigator.js';
import {OpenPdfParamsParser} from './open_pdf_params_parser.js';
import {FittingType} from './pdf_fitting_type.js';
import {DeserializeKeyEvent, LoadState, SerializeKeyEvent} from './pdf_scripting_api.js';
import {ToolbarManager} from './toolbar_manager.js';
import {LayoutOptions, Point, Viewport} from './viewport.js';
import {ViewportScroller} from './viewport_scroller.js';
import {ZoomManager} from './zoom_manager.js';

/**
 * @typedef {{
 *   source: Object,
 *   origin: string,
 *   data: !MessageData,
 * }}
 */
let MessageObject;

/**
 * @typedef {{
 *   type: string,
 *   height: number,
 *   width: number,
 *   layoutOptions: (!LayoutOptions|undefined),
 *   pageDimensions: Array
 * }}
 */
let DocumentDimensionsMessageData;

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
 *   page: number,
 *   x: number,
 *   y: number,
 *   zoom: number
 * }}
 */
let DestinationMessageData;

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

/** @return {number} Width of a scrollbar in pixels */
function getScrollbarWidth() {
  const div = document.createElement('div');
  div.style.visibility = 'hidden';
  div.style.overflow = 'scroll';
  div.style.width = '50px';
  div.style.height = '50px';
  div.style.position = 'absolute';
  document.body.appendChild(div);
  const result = div.offsetWidth - div.clientWidth;
  div.parentNode.removeChild(div);
  return result;
}

/**
 * Return the filename component of a URL, percent decoded if possible.
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
 * Whether keydown events should currently be ignored. Events are ignored when
 * an editable element has focus, to allow for proper editing controls.
 * @param {Element} activeElement The currently selected DOM node.
 * @return {boolean} True if keydown events should be ignored.
 */
export function shouldIgnoreKeyEvents(activeElement) {
  while (activeElement.shadowRoot != null &&
         activeElement.shadowRoot.activeElement != null) {
    activeElement = activeElement.shadowRoot.activeElement;
  }

  return (
      activeElement.isContentEditable ||
      (activeElement.tagName == 'INPUT' && activeElement.type != 'radio') ||
      activeElement.tagName == 'TEXTAREA');
}

/**
 * Creates a new PDFViewer. There should only be one of these objects per
 * document.
 */
export class PDFViewer {
  /**
   * @param {!BrowserApi} browserApi An object providing an API to the browser.
   */
  constructor(browserApi) {
    /** @private {!BrowserApi} */
    this.browserApi_ = browserApi;

    /** @private {string} */
    this.originalUrl_ = this.browserApi_.getStreamInfo().originalUrl;

    /** @private {string} */
    this.javascript_ = this.browserApi_.getStreamInfo().javascript || 'block';

    /** @private {!LoadState} */
    this.loadState_ = LoadState.LOADING;

    /** @private {?Object} */
    this.parentWindow_ = null;

    /** @private {?string} */
    this.parentOrigin_ = null;

    /** @private {boolean} */
    this.isFormFieldFocused_ = false;

    /** @private {number} */
    this.beepCount_ = 0;

    /** @private {!Array} */
    this.delayedScriptingMessages_ = [];

    /** @private {!PromiseResolver} */
    this.loaded_;

    /** @private {boolean} */
    this.initialLoadComplete_ = false;

    /** @private {boolean} */
    this.isPrintPreview_ = location.origin === 'chrome://print';
    document.documentElement.toggleAttribute(
        'is-print-preview', this.isPrintPreview_);

    /** @private {boolean} */
    this.isPrintPreviewLoadingFinished_ = false;

    /** @private {boolean} */
    this.isUserInitiatedEvent_ = true;

    /** @private {boolean} */
    this.hasEnteredAnnotationMode_ = false;

    /** @private {boolean} */
    this.hadPassword_ = false;

    /** @private {boolean} */
    this.canSerializeDocument_ = false;

    /** @private {!EventTracker} */
    this.tracker_ = new EventTracker();

    PDFMetrics.record(PDFMetrics.UserAction.DOCUMENT_OPENED);

    // Parse open pdf parameters.
    /** @private {!OpenPdfParamsParser} */
    this.paramsParser_ = new OpenPdfParamsParser(
        destination => this.pluginController_.getNamedDestination(destination));
    const toolbarEnabled =
        this.paramsParser_.getUiUrlParams(this.originalUrl_).toolbar &&
        !this.isPrintPreview_;

    // The sizer element is placed behind the plugin element to cause scrollbars
    // to be displayed in the window. It is sized according to the document size
    // of the pdf and zoom level.
    this.sizer_ = /** @type {!HTMLDivElement} */ ($('sizer'));

    /** @private {?ViewerPageIndicatorElement} */
    this.pageIndicator_ = this.isPrintPreview_ ?
        /** @type {!ViewerPageIndicatorElement} */ ($('page-indicator')) :
        null;

    /** @private {?ViewerPasswordScreenElement} */
    this.passwordScreen_ =
        /** @type {!ViewerPasswordScreenElement} */ ($('password-screen'));
    this.passwordScreen_.addEventListener('password-submitted', e => {
      this.onPasswordSubmitted_(
          /** @type {!CustomEvent<{password: string}>} */ (e));
    });

    /** @private {?ViewerErrorScreenElement} */
    this.errorScreen_ =
        /** @type {!ViewerErrorScreenElement} */ ($('error-screen'));
    // Can only reload if we are in a normal tab.
    if (chrome.tabs && this.browserApi_.getStreamInfo().tabId != -1) {
      this.errorScreen_.reloadFn = () => {
        chrome.tabs.reload(this.browserApi_.getStreamInfo().tabId);
      };
    }

    // Create the viewport.
    const shortWindow =
        window.innerHeight < PDFViewer.TOOLBAR_WINDOW_MIN_HEIGHT;
    const topToolbarHeight =
        (toolbarEnabled) ? PDFViewer.MATERIAL_TOOLBAR_HEIGHT : 0;
    const defaultZoom =
        this.browserApi_.getZoomBehavior() == BrowserApi.ZoomBehavior.MANAGE ?
        this.browserApi_.getDefaultZoom() :
        1.0;

    /** @private {!Viewport} */
    this.viewport_ = new Viewport(
        window, this.sizer_, getScrollbarWidth(), defaultZoom,
        topToolbarHeight);
    this.viewport_.setViewportChangedCallback(() => this.viewportChanged_());
    this.viewport_.setBeforeZoomCallback(
        () => this.currentController_.beforeZoom());
    this.viewport_.setAfterZoomCallback(
        () => this.currentController_.afterZoom());
    this.viewport_.setUserInitiatedCallback(
        userInitiated => this.setUserInitiated_(userInitiated));
    window.addEventListener('beforeunload', () => this.resetTrackers_());

    // Create the plugin object dynamically so we can set its src. The plugin
    // element is sized to fill the entire window and is set to be fixed
    // positioning, acting as a viewport. The plugin renders into this viewport
    // according to the scroll position of the window.
    /** @private {!HTMLEmbedElement} */
    this.plugin_ =
        /** @type {!HTMLEmbedElement} */ (document.createElement('embed'));

    // NOTE: The plugin's 'id' field must be set to 'plugin' since
    // chrome/renderer/printing/print_render_frame_helper.cc actually
    // references it.
    this.plugin_.id = 'plugin';
    this.plugin_.type = 'application/x-google-chrome-pdf';

    // Handle scripting messages from outside the extension that wish to
    // interact with it. We also send a message indicating that extension has
    // loaded and is ready to receive messages.
    window.addEventListener('message', message => {
      this.handleScriptingMessage(/** @type {!MessageObject} */ (message));
    }, false);

    this.plugin_.setAttribute('src', this.originalUrl_);
    this.plugin_.setAttribute(
        'stream-url', this.browserApi_.getStreamInfo().streamUrl);
    let headers = '';
    for (const header in this.browserApi_.getStreamInfo().responseHeaders) {
      headers += header + ': ' +
          this.browserApi_.getStreamInfo().responseHeaders[header] + '\n';
    }
    this.plugin_.setAttribute('headers', headers);

    this.plugin_.setAttribute('background-color', PDFViewer.BACKGROUND_COLOR);
    this.plugin_.setAttribute('top-toolbar-height', topToolbarHeight);
    this.plugin_.setAttribute('javascript', this.javascript_);

    if (this.browserApi_.getStreamInfo().embedded) {
      this.plugin_.setAttribute(
          'top-level-url', this.browserApi_.getStreamInfo().tabUrl);
    } else {
      this.plugin_.setAttribute('full-frame', '');
    }

    $('content').appendChild(this.plugin_);

    /** @private {!PluginController} */
    this.pluginController_ = new PluginController(
        this.plugin_, this.viewport_, () => this.isUserInitiatedEvent_,
        () => this.loaded);
    this.tracker_.add(
        this.pluginController_.getEventTarget(), 'plugin-message',
        e => this.handlePluginMessage_(e));

    /** @private {!InkController} */
    this.inkController_ = new InkController(this.viewport_);
    this.tracker_.add(
        this.inkController_.getEventTarget(), 'stroke-added',
        () => chrome.mimeHandlerPrivate.setShowBeforeUnloadDialog(true));
    this.tracker_.add(
        this.inkController_.getEventTarget(), 'set-annotation-undo-state',
        e => this.setAnnotationUndoState_(e));

    /** @private {!ContentController} */
    this.currentController_ = this.pluginController_;

    // Setup the button event listeners.
    /** @private {!ViewerZoomToolbarElement} */
    this.zoomToolbar_ =
        /** @type {!ViewerZoomToolbarElement} */ ($('zoom-toolbar'));
    this.zoomToolbar_.isPrintPreview = this.isPrintPreview_;
    this.zoomToolbar_.addEventListener(
        'fit-to-changed',
        e => this.fitToChanged_(
            /** @type {!CustomEvent<FitToChangedEvent>} */ (e)));
    this.zoomToolbar_.addEventListener(
        'zoom-in', () => this.viewport_.zoomIn());
    this.zoomToolbar_.addEventListener(
        'zoom-out', () => this.viewport_.zoomOut());

    /** @private {!GestureDetector} */
    this.gestureDetector_ = new GestureDetector(assert($('content')));
    this.gestureDetector_.addEventListener(
        'pinchstart', e => this.onPinchStart_(e));
    this.sentPinchEvent_ = false;
    this.gestureDetector_.addEventListener(
        'pinchupdate', e => this.onPinchUpdate_(e));
    this.gestureDetector_.addEventListener(
        'pinchend', e => this.onPinchEnd_(e));

    /** @private {?ViewerPdfToolbarElement} */
    this.toolbar_ = null;
    if (toolbarEnabled) {
      this.toolbar_ = /** @type {!ViewerPdfToolbarElement} */ ($('toolbar'));
      this.toolbar_.hidden = false;
      this.toolbar_.addEventListener('save', () => this.save_());
      this.toolbar_.addEventListener('print', () => this.print_());
      this.toolbar_.addEventListener(
          'undo', () => this.currentController_.undo());
      this.toolbar_.addEventListener(
          'redo', () => this.currentController_.redo());
      this.toolbar_.addEventListener(
          'rotate-right', () => this.rotateClockwise_());
      this.toolbar_.addEventListener('annotation-mode-toggled', e => {
        this.annotationModeToggled_(
            /** @type {!CustomEvent<{value: boolean}>} */ (e));
      });
      this.toolbar_.addEventListener(
          'annotation-tool-changed',
          e => this.inkController_.setAnnotationTool(e.detail.value));

      this.toolbar_.docTitle = getFilenameFromURL(this.originalUrl_);
    }

    document.body.addEventListener('change-page', e => {
      this.viewport_.goToPage(e.detail.page);
      if (e.detail.origin == 'bookmark') {
        PDFMetrics.record(PDFMetrics.UserAction.FOLLOW_BOOKMARK);
      } else if (e.detail.origin == 'pageselector') {
        PDFMetrics.record(PDFMetrics.UserAction.PAGE_SELECTOR_NAVIGATE);
      }
    });

    document.body.addEventListener('change-zoom', e => {
      this.viewport_.setZoom(e.detail.zoom);
    });

    document.body.addEventListener('change-page-and-xy', e => {
      const point = this.viewport_.convertPageToScreen(e.detail.page, e.detail);
      this.goToPageAndXY_(e.detail.origin, e.detail.page, point);
    });

    document.body.addEventListener('navigate', e => {
      const disposition = e.detail.newtab ?
          PdfNavigator.WindowOpenDisposition.NEW_BACKGROUND_TAB :
          PdfNavigator.WindowOpenDisposition.CURRENT_TAB;
      this.navigator_.navigate(e.detail.uri, disposition);
    });

    document.body.addEventListener('dropdown-opened', e => {
      if (e.detail == 'bookmarks') {
        PDFMetrics.record(PDFMetrics.UserAction.OPEN_BOOKMARKS_PANEL);
      }
    });

    /** @private {!ToolbarManager} */
    this.toolbarManager_ =
        new ToolbarManager(window, this.toolbar_, this.zoomToolbar_);

    // Set up the ZoomManager.
    /** @private {!ZoomManager} */
    this.zoomManager_ = ZoomManager.create(
        this.browserApi_.getZoomBehavior(), () => this.viewport_.getZoom(),
        zoom => this.browserApi_.setZoom(zoom),
        this.browserApi_.getInitialZoom());
    this.viewport_.setZoomManager(this.zoomManager_);
    this.browserApi_.addZoomEventListener(
        zoom => this.zoomManager_.onBrowserZoomChange(zoom));

    // Setup the keyboard event listener.
    document.addEventListener(
        'keydown',
        e => this.handleKeyEvent_(/** @type {!KeyboardEvent} */ (e)));
    document.addEventListener('mousemove', e => this.handleMouseEvent_(e));
    document.addEventListener('mouseout', e => this.handleMouseEvent_(e));
    document.addEventListener(
        'contextmenu', e => this.handleContextMenuEvent_(e));

    const tabId = this.browserApi_.getStreamInfo().tabId;
    /** @private {!PdfNavigator} */
    this.navigator_ = new PdfNavigator(
        this.originalUrl_, this.viewport_, this.paramsParser_,
        new NavigatorDelegate(tabId));

    /** @private {!ViewportScroller} */
    this.viewportScroller_ =
        new ViewportScroller(this.viewport_, this.plugin_, window);

    /** @private {!Array<!Bookmark>} */
    this.bookmarks_;

    /** @private {!Point} */
    this.lastViewportPosition_;

    /** @private {boolean} */
    this.inPrintPreviewMode_;

    /** @private {boolean} */
    this.dark_;

    /** @private {!DocumentDimensionsMessageData} */
    this.documentDimensions_;

    // Request translated strings.
    chrome.resourcesPrivate.getStrings(
        chrome.resourcesPrivate.Component.PDF,
        strings => this.handleStrings_(strings));

    // Listen for save commands from the browser.
    if (chrome.mimeHandlerPrivate && chrome.mimeHandlerPrivate.onSave) {
      chrome.mimeHandlerPrivate.onSave.addListener(url => this.onSave_(url));
    }
  }

  /**
   * Handle key events. These may come from the user directly or via the
   * scripting API.
   * @param {!KeyboardEvent} e the event to handle.
   * @private
   */
  handleKeyEvent_(e) {
    const position = this.viewport_.position;
    // Certain scroll events may be sent from outside of the extension.
    const fromScriptingAPI = e.fromScriptingAPI;

    if (shouldIgnoreKeyEvents(document.activeElement) || e.defaultPrevented) {
      return;
    }

    this.toolbarManager_.hideToolbarsAfterTimeout();

    const pageUpHandler = () => {
      // Go to the previous page if we are fit-to-page or fit-to-height.
      if (this.viewport_.isPagedMode()) {
        this.viewport_.goToPreviousPage();
        // Since we do the movement of the page.
        e.preventDefault();
      } else if (fromScriptingAPI) {
        position.y -= this.viewport_.size.height;
        this.viewport_.position = position;
      }
    };
    const pageDownHandler = () => {
      // Go to the next page if we are fit-to-page or fit-to-height.
      if (this.viewport_.isPagedMode()) {
        this.viewport_.goToNextPage();
        // Since we do the movement of the page.
        e.preventDefault();
      } else if (fromScriptingAPI) {
        position.y += this.viewport_.size.height;
        this.viewport_.position = position;
      }
    };

    switch (e.keyCode) {
      case 9:  // Tab key.
        this.toolbarManager_.showToolbarsForKeyboardNavigation();
        return;
      case 27:  // Escape key.
        if (!this.isPrintPreview_) {
          this.toolbarManager_.hideSingleToolbarLayer();
          return;
        }
        break;  // Ensure escape falls through to the print-preview handler.
      case 32:  // Space key.
        if (e.shiftKey) {
          pageUpHandler();
        } else {
          pageDownHandler();
        }
        return;
      case 33:  // Page up key.
        pageUpHandler();
        return;
      case 34:  // Page down key.
        pageDownHandler();
        return;
      case 37:  // Left arrow key.
        if (!hasKeyModifiers(e)) {
          // Go to the previous page if there are no horizontal scrollbars and
          // no form field is focused.
          if (!(this.viewport_.documentHasScrollbars().horizontal ||
                this.isFormFieldFocused_)) {
            this.viewport_.goToPreviousPage();
            // Since we do the movement of the page.
            e.preventDefault();
          } else if (fromScriptingAPI) {
            position.x -= Viewport.SCROLL_INCREMENT;
            this.viewport_.position = position;
          }
        }
        return;
      case 38:  // Up arrow key.
        if (fromScriptingAPI) {
          position.y -= Viewport.SCROLL_INCREMENT;
          this.viewport_.position = position;
        }
        return;
      case 39:  // Right arrow key.
        if (!hasKeyModifiers(e)) {
          // Go to the next page if there are no horizontal scrollbars and no
          // form field is focused.
          if (!(this.viewport_.documentHasScrollbars().horizontal ||
                this.isFormFieldFocused_)) {
            this.viewport_.goToNextPage();
            // Since we do the movement of the page.
            e.preventDefault();
          } else if (fromScriptingAPI) {
            position.x += Viewport.SCROLL_INCREMENT;
            this.viewport_.position = position;
          }
        }
        return;
      case 40:  // Down arrow key.
        if (fromScriptingAPI) {
          position.y += Viewport.SCROLL_INCREMENT;
          this.viewport_.position = position;
        }
        return;
      case 65:  // 'a' key.
        if (e.ctrlKey || e.metaKey) {
          this.pluginController_.selectAll();
          // Since we do selection ourselves.
          e.preventDefault();
        }
        return;
      case 71:  // 'g' key.
        if (this.toolbar_ && (e.ctrlKey || e.metaKey) && e.altKey) {
          this.toolbarManager_.showToolbars();
          this.toolbar_.selectPageNumber();
        }
        return;
      case 219:  // Left bracket key.
        if (e.ctrlKey) {
          this.rotateCounterclockwise_();
        }
        return;
      case 220:  // Backslash key.
        if (e.ctrlKey) {
          this.zoomToolbar_.fitToggleFromHotKey();
        }
        return;
      case 221:  // Right bracket key.
        if (e.ctrlKey) {
          this.rotateClockwise_();
        }
        return;
    }

    // Give print preview a chance to handle the key event.
    if (!fromScriptingAPI && this.isPrintPreview_) {
      this.sendScriptingMessage_(
          {type: 'sendKeyEvent', keyEvent: SerializeKeyEvent(e)});
    } else {
      // Show toolbars as a fallback.
      if (!(e.shiftKey || e.ctrlKey || e.altKey)) {
        this.toolbarManager_.showToolbars();
      }
    }
  }

  handleMouseEvent_(e) {
    if (e.type == 'mousemove') {
      this.toolbarManager_.handleMouseMove(e);
    } else if (e.type == 'mouseout') {
      this.toolbarManager_.hideToolbarsForMouseOut();
    }
  }

  /**
   * @param {!Event} e The context menu event
   * @private
   */
  handleContextMenuEvent_(e) {
    // Stop Chrome from popping up the context menu on long press. We need to
    // make sure the start event did not have 2 touches because we don't want
    // to block two finger tap opening the context menu. We check for
    // firesTouchEvents in order to not block the context menu on right click.
    const capabilities =
        /** @type {{ sourceCapabilities: Object }} */ (e).sourceCapabilities;
    if (capabilities.firesTouchEvents &&
        !this.gestureDetector_.wasTwoFingerTouch()) {
      e.preventDefault();
    }
  }

  /**
   * Handles the annotation mode being toggled on or off.
   * @param {!CustomEvent<{value: boolean}>} e
   * @private
   */
  async annotationModeToggled_(e) {
    const annotationMode = e.detail.value;
    if (annotationMode) {
      // Enter annotation mode.
      assert(this.currentController_ == this.pluginController_);
      // TODO(dstockwell): set plugin read-only, begin transition
      this.updateProgress_(0);
      // TODO(dstockwell): handle save failure
      const saveResult = await this.pluginController_.save(true);
      // Data always exists when save is called with requireResult = true.
      const result = /** @type {!RequiredSaveResult} */ (saveResult);
      if (result.hasUnsavedChanges) {
        assert(!loadTimeData.getBoolean('pdfFormSaveEnabled'));
        try {
          await $('form-warning').show();
        } catch (e) {
          // The user aborted entering annotation mode. Revert to the plugin.
          this.toolbar_.annotationMode = false;
          this.updateProgress_(100);
          return;
        }
      }
      PDFMetrics.record(PDFMetrics.UserAction.ENTER_ANNOTATION_MODE);
      this.hasEnteredAnnotationMode_ = true;
      // TODO(dstockwell): feed real progress data from the Ink component
      this.updateProgress_(50);
      await this.inkController_.load(result.fileName, result.dataToSave);
      this.inkController_.setAnnotationTool(
          assert(this.toolbar_.annotationTool));
      this.currentController_ = this.inkController_;
      this.pluginController_.unload();
      this.updateProgress_(100);
    } else {
      // Exit annotation mode.
      PDFMetrics.record(PDFMetrics.UserAction.EXIT_ANNOTATION_MODE);
      assert(this.currentController_ == this.inkController_);
      // TODO(dstockwell): set ink read-only, begin transition
      this.updateProgress_(0);
      // This runs separately to allow other consumers of `loaded` to queue
      // up after this task.
      this.loaded.then(() => {
        this.currentController_ = this.pluginController_;
        this.inkController_.unload();
      });
      // TODO(dstockwell): handle save failure
      const saveResult = await this.inkController_.save(true);
      // Data always exists when save is called with requireResult = true.
      const result = /** @type {!RequiredSaveResult} */ (saveResult);
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
    if (!this.toolbar_.annotationMode) {
      return;
    }
    this.toolbar_.toggleAnnotation();
    await this.loaded;
  }

  /**
   * Request to change the viewport fitting type.
   * @param {!CustomEvent<FitToChangedEvent>} e
   * @private
   */
  fitToChanged_(e) {
    if (e.detail.fittingType == FittingType.FIT_TO_PAGE) {
      this.viewport_.fitToPage();
      this.toolbarManager_.forceHideTopToolbar();
    } else if (e.detail.fittingType == FittingType.FIT_TO_WIDTH) {
      this.viewport_.fitToWidth();
    } else if (e.detail.fittingType == FittingType.FIT_TO_HEIGHT) {
      this.viewport_.fitToHeight();
      this.toolbarManager_.forceHideTopToolbar();
    }

    if (e.detail.userInitiated) {
      PDFMetrics.recordFitTo(e.detail.fittingType);
    }
  }

  /**
   * Sends a 'documentLoaded' message to the PDFScriptingAPI if the document has
   * finished loading.
   * @private
   */
  sendDocumentLoadedMessage_() {
    if (this.loadState_ == LoadState.LOADING) {
      return;
    }
    if (this.isPrintPreview_ && !this.isPrintPreviewLoadingFinished_) {
      return;
    }
    this.sendScriptingMessage_(
        {type: 'documentLoaded', load_state: this.loadState_});
  }

  /**
   * Handle open pdf parameters. This function updates the viewport as per
   * the parameters mentioned in the url while opening pdf. The order is
   * important as later actions can override the effects of previous actions.
   * @param {Object} params The open params passed in the URL.
   * @private
   */
  handleURLParams_(params) {
    if (params.zoom) {
      this.viewport_.setZoom(params.zoom);
    }

    if (params.position) {
      this.viewport_.goToPageAndXY(
          params.page ? params.page : 0, params.position.x, params.position.y);
    } else if (params.page) {
      this.viewport_.goToPage(params.page);
    }

    if (params.view) {
      this.isUserInitiatedEvent_ = false;
      this.zoomToolbar_.forceFit(params.view);
      if (params.viewPosition) {
        const zoomedPositionShift =
            params.viewPosition * this.viewport_.getZoom();
        const currentViewportPosition = this.viewport_.position;
        if (params.view == FittingType.FIT_TO_WIDTH) {
          currentViewportPosition.y += zoomedPositionShift;
        } else if (params.view == FittingType.FIT_TO_HEIGHT) {
          currentViewportPosition.x += zoomedPositionShift;
        }
        this.viewport_.position = currentViewportPosition;
      }
      this.isUserInitiatedEvent_ = true;
    }
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
    this.viewport_.goToPageAndXY(page, message.x, message.y);
    if (origin == 'bookmark') {
      PDFMetrics.record(PDFMetrics.UserAction.FOLLOW_BOOKMARK);
    }
  }

  /**
   * @return {?Promise} Resolved when the load state reaches LOADED,
   * rejects on FAILED. Returns null if no promise has been created, which
   * is the case for initial load of the PDF.
   */
  get loaded() {
    return this.loaded_ ? this.loaded_.promise : null;
  }

  /** @return {!Viewport} The viewport. Used for testing. */
  get viewport() {
    return this.viewport_;
  }

  /** @return {!Array<!Bookmark>} The bookmarks. Used for testing. */
  get bookmarks() {
    return this.bookmarks_;
  }

  /**
   * Updates the load state and triggers completion of the `loaded`
   * promise if necessary.
   * @param {!LoadState} loadState
   * @private
   */
  setLoadState_(loadState) {
    if (this.loadState_ == loadState) {
      return;
    }
    assert(
        loadState == LoadState.LOADING || this.loadState_ == LoadState.LOADING);
    this.loadState_ = loadState;
    if (!this.initialLoadComplete_) {
      this.initialLoadComplete_ = true;
      return;
    }
    if (loadState == LoadState.SUCCESS) {
      this.loaded_.resolve();
    } else if (loadState == LoadState.FAILED) {
      this.loaded_.reject();
    } else {
      this.loaded_ = new PromiseResolver();
    }
  }

  /**
   * Update the loading progress of the document in response to a progress
   * message being received from the content controller.
   * @param {number} progress the progress as a percentage.
   * @private
   */
  updateProgress_(progress) {
    if (this.toolbar_) {
      this.toolbar_.loadProgress = progress;
    }

    if (progress == -1) {
      // Document load failed.
      this.errorScreen_.show();
      this.sizer_.style.display = 'none';
      if (this.passwordScreen_.active) {
        this.passwordScreen_.deny();
        this.passwordScreen_.close();
      }
      this.setLoadState_(LoadState.FAILED);
      this.isPrintPreviewLoadingFinished_ = true;
      this.sendDocumentLoadedMessage_();
    } else if (progress == 100) {
      // Document load complete.
      if (this.lastViewportPosition_) {
        this.viewport_.position = this.lastViewportPosition_;
      }
      this.paramsParser_.getViewportFromUrlParams(
          this.originalUrl_, params => this.handleURLParams_(params));
      this.setLoadState_(LoadState.SUCCESS);
      this.sendDocumentLoadedMessage_();
      while (this.delayedScriptingMessages_.length > 0) {
        this.handleScriptingMessage(this.delayedScriptingMessages_.shift());
      }

      this.toolbarManager_.hideToolbarsAfterTimeout();
    } else {
      this.setLoadState_(LoadState.LOADING);
    }
  }

  /** @private */
  sendBackgroundColorForPrintPreview_() {
    this.pluginController_.backgroundColorChanged(
        this.dark_ ? PDFViewer.PRINT_PREVIEW_DARK_BACKGROUND_COLOR :
                     PDFViewer.PRINT_PREVIEW_BACKGROUND_COLOR);
  }

  /**
   * Load a dictionary of translated strings into the UI. Used as a callback for
   * chrome.resourcesPrivate.
   * @param {Object} strings Dictionary of translated strings
   * @private
   */
  handleStrings_(strings) {
    const stringsDictionary =
        /** @type {{ textdirection: string, language: string }} */ (strings);
    document.documentElement.dir = stringsDictionary.textdirection;
    document.documentElement.lang = stringsDictionary.language;

    loadTimeData.data = strings;
    if (this.isPrintPreview_) {
      this.sendBackgroundColorForPrintPreview_();
    }

    $('toolbar').strings = strings;
    $('toolbar').pdfAnnotationsEnabled =
        loadTimeData.getBoolean('pdfAnnotationsEnabled');
    $('toolbar').printingEnabled = loadTimeData.getBoolean('printingEnabled');
    $('zoom-toolbar').setStrings(strings);
    $('password-screen').strings = strings;
    $('error-screen').strings = strings;
    if ($('form-warning')) {
      $('form-warning').strings = strings;
    }
  }

  /**
   * An event handler for handling password-submitted events. These are fired
   * when an event is entered into the password screen.
   * @param {!CustomEvent<{password: string}>} event a password-submitted event.
   * @private
   */
  onPasswordSubmitted_(event) {
    this.pluginController_.getPasswordComplete(event.detail.password);
  }

  /**
   * A callback that sets |isUserInitiatedEvent_| to |userInitiated|.
   * @param {boolean} userInitiated The value to set |isUserInitiatedEvent_| to.
   * @private
   */
  setUserInitiated_(userInitiated) {
    assert(this.isUserInitiatedEvent_ != userInitiated);
    this.isUserInitiatedEvent_ = userInitiated;
  }

  /**
   * A callback that's called when an update to a pinch zoom is detected.
   * @param {!Object} e the pinch event.
   * @private
   */
  onPinchUpdate_(e) {
    // Throttle number of pinch events to one per frame.
    if (!this.sentPinchEvent_) {
      this.sentPinchEvent_ = true;
      window.requestAnimationFrame(() => {
        this.sentPinchEvent_ = false;
        this.viewport_.pinchZoom(e);
      });
    }
  }

  /**
   * A callback that's called when the end of a pinch zoom is detected.
   * @param {!Object} e the pinch event.
   * @private
   */
  onPinchEnd_(e) {
    // Using rAF for pinch end prevents pinch updates scheduled by rAF getting
    // sent after the pinch end.
    window.requestAnimationFrame(() => {
      this.viewport_.pinchZoomEnd(e);
    });
  }

  /**
   * A callback that's called when the start of a pinch zoom is detected.
   * @param {!Object} e the pinch event.
   * @private
   */
  onPinchStart_(e) {
    // We also use rAF for pinch start, so that if there is a pinch end event
    // scheduled by rAF, this pinch start will be sent after.
    window.requestAnimationFrame(() => {
      this.viewport_.pinchZoomStart(e);
    });
  }

  /**
   * A callback that's called after the viewport changes.
   * @private
   */
  viewportChanged_() {
    if (!this.documentDimensions_) {
      return;
    }

    // Offset the toolbar position so that it doesn't move if scrollbars appear.
    const hasScrollbars = this.viewport_.documentHasScrollbars();
    const scrollbarWidth = this.viewport_.scrollbarWidth;
    const verticalScrollbarWidth = hasScrollbars.vertical ? scrollbarWidth : 0;
    const horizontalScrollbarWidth =
        hasScrollbars.horizontal ? scrollbarWidth : 0;

    // Shift the zoom toolbar to the left by half a scrollbar width. This
    // gives a compromise: if there is no scrollbar visible then the toolbar
    // will be half a scrollbar width further left than the spec but if there
    // is a scrollbar visible it will be half a scrollbar width further right
    // than the spec. In RTL layout normally, and in LTR layout in Print Preview
    // when the NewPrintPreview flag is enabled, the zoom toolbar is on the left
    // left side, but the scrollbar is still on the right, so this is not
    // necessary.
    if (isRTL() === this.isPrintPreview_) {
      this.zoomToolbar_.style.right =
          -verticalScrollbarWidth + (scrollbarWidth / 2) + 'px';
    }
    // Having a horizontal scrollbar is much rarer so we don't offset the
    // toolbar from the bottom any more than what the spec says. This means
    // that when there is a scrollbar visible, it will be a full scrollbar
    // width closer to the bottom of the screen than usual, but this is ok.
    this.zoomToolbar_.style.bottom = -horizontalScrollbarWidth + 'px';

    // Update the page indicator.
    const visiblePage = this.viewport_.getMostVisiblePage();

    if (this.toolbar_) {
      this.toolbar_.pageNo = visiblePage + 1;
    }

    // TODO(raymes): Give pageIndicator_ the same API as toolbar_.
    if (this.pageIndicator_) {
      const lastIndex = this.pageIndicator_.index;
      this.pageIndicator_.index = visiblePage;
      if (this.documentDimensions_.pageDimensions.length > 1 &&
          hasScrollbars.vertical && lastIndex !== undefined) {
        this.pageIndicator_.style.visibility = 'visible';
      } else {
        this.pageIndicator_.style.visibility = 'hidden';
      }
    }

    this.currentController_.viewportChanged();

    const visiblePageDimensions = this.viewport_.getPageScreenRect(visiblePage);
    const size = this.viewport_.size;
    this.sendScriptingMessage_({
      type: 'viewport',
      pageX: visiblePageDimensions.x,
      pageY: visiblePageDimensions.y,
      pageWidth: visiblePageDimensions.width,
      viewportWidth: size.width,
      viewportHeight: size.height
    });
  }

  /**
   * Handle a scripting message from outside the extension (typically sent by
   * PDFScriptingAPI in a page containing the extension) to interact with the
   * plugin.
   * @param {!MessageObject} message The message to handle.
   */
  handleScriptingMessage(message) {
    if (this.parentWindow_ != message.source) {
      this.parentWindow_ = message.source;
      this.parentOrigin_ = message.origin;
      // Ensure that we notify the embedder if the document is loaded.
      if (this.loadState_ != LoadState.LOADING) {
        this.sendDocumentLoadedMessage_();
      }
    }

    if (this.handlePrintPreviewScriptingMessage_(message)) {
      return;
    }

    // Delay scripting messages from users of the scripting API until the
    // document is loaded. This simplifies use of the APIs.
    if (this.loadState_ != LoadState.SUCCESS) {
      this.delayedScriptingMessages_.push(message);
      return;
    }

    switch (message.data.type.toString()) {
      case 'getSelectedText':
        this.pluginController_.getSelectedText();
        break;
      case 'print':
        this.pluginController_.print();
        break;
      case 'selectAll':
        this.pluginController_.selectAll();
        break;
    }
  }

  /**
   * Handle scripting messages specific to print preview.
   * @param {!MessageObject} message the message to handle.
   * @return {boolean} true if the message was handled, false otherwise.
   * @private
   */
  handlePrintPreviewScriptingMessage_(message) {
    if (!this.isPrintPreview_) {
      return false;
    }

    let messageData = message.data;
    switch (messageData.type.toString()) {
      case 'loadPreviewPage':
        messageData =
            /** @type {{ url:  string, index: number }} */ (messageData);
        this.pluginController_.loadPreviewPage(
            messageData.url, messageData.index);
        return true;
      case 'resetPrintPreviewMode':
        messageData = /** @type {!PrintPreviewParams} */ (messageData);
        this.setLoadState_(LoadState.LOADING);
        if (!this.inPrintPreviewMode_) {
          this.inPrintPreviewMode_ = true;
          this.isUserInitiatedEvent_ = false;
          this.zoomToolbar_.forceFit(FittingType.FIT_TO_PAGE);
          this.isUserInitiatedEvent_ = true;
        }

        // Stash the scroll location so that it can be restored when the new
        // document is loaded.
        this.lastViewportPosition_ = this.viewport_.position;

        // TODO(raymes): Disable these properly in the plugin.
        const printButton = $('print-button');
        if (printButton) {
          printButton.parentNode.removeChild(printButton);
        }
        const saveButton = $('save-button');
        if (saveButton) {
          saveButton.parentNode.removeChild(saveButton);
        }

        this.pageIndicator_.pageLabels = messageData.pageNumbers;

        this.pluginController_.resetPrintPreviewMode(messageData);
        return true;
      case 'sendKeyEvent':
        this.handleKeyEvent_(/** @type {!KeyboardEvent} */ (DeserializeKeyEvent(
            /** @type {{ keyEvent: Object }} */ (message.data).keyEvent)));
        return true;
      case 'hideToolbars':
        this.toolbarManager_.resetKeyboardNavigationAndHideToolbars();
        return true;
      case 'darkModeChanged':
        this.dark_ = /** @type {{darkMode: boolean}} */ (message.data).darkMode;
        if (this.isPrintPreview_) {
          this.sendBackgroundColorForPrintPreview_();
        }
        return true;
      case 'scrollPosition':
        const position = this.viewport_.position;
        messageData = /** @type {{ x: number, y: number }} */ (message.data);
        position.y += messageData.y;
        position.x += messageData.x;
        this.viewport_.position = position;
        return true;
    }

    return false;
  }

  /**
   * Send a scripting message outside the extension (typically to
   * PDFScriptingAPI in a page containing the extension).
   * @param {Object} message the message to send.
   * @private
   */
  sendScriptingMessage_(message) {
    if (this.parentWindow_ && this.parentOrigin_) {
      let targetOrigin;
      // Only send data back to the embedder if it is from the same origin,
      // unless we're sending it to ourselves (which could happen in the case
      // of tests). We also allow documentLoaded messages through as this won't
      // leak important information.
      if (this.parentOrigin_ == window.location.origin) {
        targetOrigin = this.parentOrigin_;
      } else if (message.type == 'documentLoaded') {
        targetOrigin = '*';
      } else {
        targetOrigin = this.originalUrl_;
      }
      try {
        this.parentWindow_.postMessage(message, targetOrigin);
      } catch (ok) {
        // TODO(crbug.com/1004425): targetOrigin probably was rejected, such as
        // a "data:" URL. This shouldn't cause this method to throw, though.
      }
    }
  }

  /**
   * @param {!CustomEvent<MessageData>} e
   * @private
   */
  handlePluginMessage_(e) {
    const data = e.detail;
    switch (data.type.toString()) {
      case 'beep':
        this.handleBeep_();
        return;
      case 'documentDimensions':
        this.setDocumentDimensions_(
            /** @type {!DocumentDimensionsMessageData} */ (data));
        return;
      case 'getPassword':
        this.handlePasswordRequest_();
        return;
      case 'getSelectedTextReply':
        this.handleSelectedTextReply_(
            /** @type {{ selectedText: string }} */ (data).selectedText);
        return;
      case 'loadProgress':
        this.updateProgress_(
            /** @type {{ progress: number }} */ (data).progress);
        return;
      case 'navigate':
        const navigateData = /** @type {!NavigateMessageData} */ (data);
        this.handleNavigate_(navigateData.url, navigateData.disposition);
        return;
      case 'navigateToDestination':
        const destinationData = /** @type {!DestinationMessageData} */ (data);
        this.handleNavigateToDestination_(
            destinationData.page, destinationData.x, destinationData.y,
            destinationData.zoom);
        return;
      case 'printPreviewLoaded':
        this.handlePrintPreviewLoaded_();
        return;
      case 'metadata':
        const metadata = /** @type {!MetadataMessageData} */ (data);
        this.setDocumentMetadata_(
            metadata.title, metadata.bookmarks, metadata.canSerializeDocument);
        return;
      case 'setIsSelecting':
        this.setIsSelecting_(
            /** @type {{ isSelecting: boolean }} */ (data).isSelecting);
        return;
      case 'getNamedDestinationReply':
        this.paramsParser_.onNamedDestinationReceived(
            /** @type {{ pageNumber: number }} */ (data).pageNumber);
        return;
      case 'formFocusChange':
        this.isFormFieldFocused_ =
            /** @type {{ focused: boolean }} */ (data).focused;
        return;
    }
    assertNotReached('Unknown message type received: ' + data.type);
  }

  /**
   * Sets document dimensions from the current controller.
   * @param {!DocumentDimensionsMessageData} documentDimensions
   * @private
   */
  setDocumentDimensions_(documentDimensions) {
    this.documentDimensions_ = documentDimensions;
    this.isUserInitiatedEvent_ = false;
    this.viewport_.setDocumentDimensions(this.documentDimensions_);
    this.isUserInitiatedEvent_ = true;
    // If we received the document dimensions, the password was good so we
    // can dismiss the password screen.
    if (this.passwordScreen_.active) {
      this.passwordScreen_.close();
    }

    if (this.toolbar_) {
      this.toolbar_.docLength = this.documentDimensions_.pageDimensions.length;
    }
  }

  /**
   * Handles a beep request from the current controller.
   * @private
   */
  handleBeep_() {
    // Beeps are annoying, so just track count for now.
    this.beepCount_ += 1;
  }

  /**
   * Handles a password request from the current controller.
   * @private
   */
  handlePasswordRequest_() {
    // If the password screen isn't up, put it up. Otherwise we're
    // responding to an incorrect password so deny it.
    if (!this.passwordScreen_.active) {
      this.hadPassword_ = true;
      this.updateAnnotationAvailable_();
      this.passwordScreen_.show();
    } else {
      this.passwordScreen_.deny();
    }
  }

  /**
   * Handles a selected text reply from the current controller.
   * @param {string} selectedText
   * @private
   */
  handleSelectedTextReply_(selectedText) {
    this.sendScriptingMessage_({
      type: 'getSelectedTextReply',
      selectedText: selectedText,
    });
  }

  /**
   * Handles a navigation request from the current controller.
   * @param {string} url
   * @param {!PdfNavigator.WindowOpenDisposition} disposition
   * @private
   */
  handleNavigate_(url, disposition) {
    // If in print preview, always open a new tab.
    if (this.isPrintPreview_) {
      this.navigator_.navigate(
          url, PdfNavigator.WindowOpenDisposition.NEW_BACKGROUND_TAB);
    } else {
      this.navigator_.navigate(url, disposition);
    }
  }

  /**
   * Handles an internal navigation request to a destination from the current
   * controller.
   *
   * @param {number} page
   * @param {number} x
   * @param {number} y
   * @param {number} zoom
   * @private
   */
  handleNavigateToDestination_(page, x, y, zoom) {
    if (zoom) {
      this.viewport_.setZoom(zoom);
    }

    if (x || y) {
      this.viewport_.goToPageAndXY(page, x ? x : 0, y ? y : 0);
    } else {
      this.viewport_.goToPage(page);
    }
  }

  /**
   * Handles a notification that print preview has loaded from the
   * current controller.
   * @private
   */
  handlePrintPreviewLoaded_() {
    this.isPrintPreviewLoadingFinished_ = true;
    this.sendDocumentLoadedMessage_();
  }

  /**
   * Sets document metadata from the current controller.
   * @param {string} title
   * @param {!Array<!Bookmark>} bookmarks
   * @param {boolean} canSerializeDocument
   * @private
   */
  setDocumentMetadata_(title, bookmarks, canSerializeDocument) {
    if (title) {
      document.title = title;
    } else {
      document.title = getFilenameFromURL(this.originalUrl_);
    }
    this.bookmarks_ = bookmarks;
    if (this.toolbar_) {
      this.toolbar_.docTitle = document.title;
      this.toolbar_.bookmarks = this.bookmarks_;
    }
    this.canSerializeDocument_ = canSerializeDocument;
    this.updateAnnotationAvailable_();
  }

  /**
   * Sets the is selecting flag from the current controller.
   * @param {boolean} isSelecting
   * @private
   */
  setIsSelecting_(isSelecting) {
    this.viewportScroller_.setEnableScrolling(isSelecting);
  }

  /**
   * An event handler for when the browser tells the PDF Viewer to perform a
   * save.
   * @param {string} streamUrl unique identifier for a PDF Viewer instance.
   * @private
   */
  async onSave_(streamUrl) {
    if (streamUrl != this.browserApi_.getStreamInfo().streamUrl) {
      return;
    }

    this.save_();
  }

  /**
   * Saves the current PDF document to disk.
   * @private
   */
  async save_() {
    PDFMetrics.record(PDFMetrics.UserAction.SAVE);
    if (this.hasEnteredAnnotationMode_) {
      PDFMetrics.record(PDFMetrics.UserAction.SAVE_WITH_ANNOTATION);
    }
    // If we have entered annotation mode we must require the local
    // contents to ensure annotations are saved. Otherwise we would
    // save the cached or remote copy without annotatios.
    const requireResult = this.hasEnteredAnnotationMode_;
    // TODO(dstockwell): Report an error to user if this fails.
    const result = await this.currentController_.save(requireResult);
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
        {type: 'saveFile', suggestedName: fileName}, entry => {
          if (chrome.runtime.lastError) {
            if (chrome.runtime.lastError.message != 'User cancelled') {
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

    // Saving in Annotation mode is destructive: crbug.com/919364
    this.exitAnnotationMode_();
  }

  /** @private */
  async print_() {
    PDFMetrics.record(PDFMetrics.UserAction.PRINT);
    await this.exitAnnotationMode_();
    this.currentController_.print();
  }

  /**
   * Updates the toolbar's annotation available flag depending on current
   * conditions.
   * @private
   */
  updateAnnotationAvailable_() {
    if (!this.toolbar_) {
      return;
    }
    let annotationAvailable = true;
    if (this.viewport_.getClockwiseRotations() != 0) {
      annotationAvailable = false;
    }
    if (this.hadPassword_) {
      annotationAvailable = false;
    }
    if (!this.canSerializeDocument_) {
      annotationAvailable = false;
    }
    this.toolbar_.annotationAvailable = annotationAvailable;
  }

  /** @private */
  rotateClockwise_() {
    PDFMetrics.record(PDFMetrics.UserAction.ROTATE);
    this.viewport_.rotateClockwise();
    this.currentController_.rotateClockwise();
    this.updateAnnotationAvailable_();
  }

  /** @private */
  rotateCounterclockwise_() {
    PDFMetrics.record(PDFMetrics.UserAction.ROTATE);
    this.viewport_.rotateCounterclockwise();
    this.currentController_.rotateCounterclockwise();
    this.updateAnnotationAvailable_();
  }

  /**
   * @param {!CustomEvent<{canUndo: boolean, canRedo: boolean}>} e
   * @private
   */
  setAnnotationUndoState_(e) {
    this.toolbar_.canUndoAnnotation = e.detail.canUndo;
    this.toolbar_.canRedoAnnotation = e.detail.canRedo;
  }

  /** @private */
  resetTrackers_() {
    this.viewport_.resetTracker();
    if (this.tracker_) {
      this.tracker_.removeAll();
    }
  }
}

// Export on |window| such that scripts injected from pdf_extension_test.cc can
// access it.
window.PDFViewer = PDFViewer;

/**
 * The height of the toolbar along the top of the page. The document will be
 * shifted down by this much in the viewport.
 */
PDFViewer.MATERIAL_TOOLBAR_HEIGHT = 56;

/**
 * Minimum height for the material toolbar to show (px). Should match the media
 * query in index-material.css. If the window is smaller than this at load,
 * leave no space for the toolbar.
 */
PDFViewer.TOOLBAR_WINDOW_MIN_HEIGHT = 250;

/**
 * The background color used for print preview (--google-grey-refresh-300).
 */
PDFViewer.PRINT_PREVIEW_BACKGROUND_COLOR = '0xFFDADCE0';

/**
 * The background color used for print preview when dark mode is enabled
 * (--google-grey-refresh-700).
 */
PDFViewer.PRINT_PREVIEW_DARK_BACKGROUND_COLOR = '0xFF5F6368';

/**
 * The background color used for the regular viewer.
 */
PDFViewer.BACKGROUND_COLOR = '0xFF525659';
