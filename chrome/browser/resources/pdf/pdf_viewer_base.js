// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertNotReached} from 'chrome://resources/js/assert.m.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserApi, ZoomBehavior} from './browser_api.js';
import {FittingType, Point} from './constants.js';
import {ContentController, MessageData, PluginController} from './controller.js';
import {PDFMetrics, UserAction} from './metrics.js';
import {OpenPdfParamsParser} from './open_pdf_params_parser.js';
import {LoadState} from './pdf_scripting_api.js';
import {DocumentDimensionsMessageData, MessageObject} from './pdf_viewer_utils.js';
import {Viewport} from './viewport.js';
import {ViewportScroller} from './viewport_scroller.js';
import {ZoomManager} from './zoom_manager.js';

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

export class PDFViewerBaseElement extends PolymerElement {
  static get is() {
    return 'pdf-viewer-base';
  }

  static get template() {
    return null;
  }

  static get properties() {
    return {
      strings: Object,
    };
  }

  constructor() {
    super();

    /** @protected {Object|undefined} */
    this.strings = undefined;

    /** @protected {?BrowserApi} */
    this.browserApi = null;

    /** @protected {?ContentController} */
    this.currentController = null;

    /** @protected {string} */
    this.originalUrl = '';

    /** @protected {!EventTracker} */
    this.tracker = new EventTracker();

    /** @protected {boolean} */
    this.isUserInitiatedEvent = true;

    /** @protected {?Point} */
    this.lastViewportPosition = null;

    /** @protected {?OpenPdfParamsParser} */
    this.paramsParser = null;

    /** @protected {?ViewportScroller} */
    this.viewportScroller = null;

    /** @protected {?DocumentDimensionsMessageData} */
    this.documentDimensions = null;

    /** @private {boolean} */
    this.overrideSendScriptingMessageForTest_ = false;

    /** @private {!LoadState} */
    this.loadState_ = LoadState.LOADING;

    /** @private {?Object} */
    this.parentWindow_ = null;

    /** @private {?string} */
    this.parentOrigin_ = null;

    /** @private {!Array} */
    this.delayedScriptingMessages_ = [];

    /** @private {?PromiseResolver} */
    this.loaded_ = null;

    /** @private {boolean} */
    this.initialLoadComplete_ = false;

    /** @private {?Viewport} */
    this.viewport_ = null;

    /** @private {?PluginController} */
    this.pluginController_ = null;

    /** @private {?HTMLEmbedElement} */
    this.plugin_ = null;

    /** @private {?ZoomManager} */
    this.zoomManager_ = null;
  }

  /** @return {number} The height of the top toolbar */
  getToolbarHeight() {
    return 0;
  }

  /** @return {boolean} Whether the top toolbar is fixed (does not auto-hide) */
  hasFixedToolbar() {
    return false;
  }

  /**
   * @return {!HTMLDivElement}
   * @protected
   */
  getContent() {}

  /**
   * @return {!HTMLDivElement}
   * @protected
   */
  getSizer() {}

  /**
   * @return {!ViewerErrorScreenElement}
   * @protected
   */
  getErrorScreen() {}

  /**
   * @param {!FittingType} view
   * @protected
   */
  forceFit(view) {}

  /**
   * @param {number} viewportZoom
   * @protected
   */
  afterZoom(viewportZoom) {}

  /**
   * @param {string} query
   * @return {?Element}
   * @protected
   */
  $$(query) {
    return this.shadowRoot.querySelector(query);
  }

  /** @return {string} */
  getBackgroundColor() {
    return '';
  }

  /**
   * @param {boolean} pdfViewerUpdateEnabled is the feature is enabled.
   * @return {!HTMLEmbedElement} The plugin
   * @private
   */
  createPlugin_(pdfViewerUpdateEnabled) {
    // Create the plugin object dynamically so we can set its src. The plugin
    // element is sized to fill the entire window and is set to be fixed
    // positioning, acting as a viewport. The plugin renders into this viewport
    // according to the scroll position of the window.
    const plugin =
        /** @type {!HTMLEmbedElement} */ (document.createElement('embed'));

    // NOTE: The plugin's 'id' field must be set to 'plugin' since
    // ChromePrintRenderFrameHelperDeleage::GetPdfElement() in
    // chrome/renderer/printing/chrome_print_render_frame_helper_delegate.cc
    // actually references it.
    plugin.id = 'plugin';
    plugin.type = 'application/x-google-chrome-pdf';

    plugin.setAttribute('src', this.originalUrl);
    plugin.setAttribute(
        'stream-url', this.browserApi.getStreamInfo().streamUrl);
    let headers = '';
    for (const header in this.browserApi.getStreamInfo().responseHeaders) {
      headers += header + ': ' +
          this.browserApi.getStreamInfo().responseHeaders[header] + '\n';
    }
    plugin.setAttribute('headers', headers);

    plugin.setAttribute('background-color', this.getBackgroundColor());
    plugin.setAttribute('top-toolbar-height', this.getToolbarHeight());

    const javascript = this.browserApi.getStreamInfo().javascript || 'block';
    plugin.setAttribute('javascript', javascript);

    if (this.browserApi.getStreamInfo().embedded) {
      plugin.setAttribute(
          'top-level-url', this.browserApi.getStreamInfo().tabUrl);
    } else {
      plugin.toggleAttribute('full-frame', true);
    }

    if (pdfViewerUpdateEnabled) {
      plugin.toggleAttribute('pdf-viewer-update-enabled', true);
    }

    return plugin;
  }

  /** @param {!BrowserApi} browserApi */
  init(browserApi) {
    this.browserApi = browserApi;
    this.originalUrl = this.browserApi.getStreamInfo().originalUrl;

    PDFMetrics.record(UserAction.DOCUMENT_OPENED);

    // Parse open pdf parameters.
    this.paramsParser = new OpenPdfParamsParser(destination => {
      return this.pluginController_.getNamedDestination(destination);
    });

    // Can only reload if we are in a normal tab.
    if (chrome.tabs && this.browserApi.getStreamInfo().tabId !== -1) {
      this.getErrorScreen().reloadFn = () => {
        chrome.tabs.reload(this.browserApi.getStreamInfo().tabId);
      };
    }

    // Determine the scrolling container.
    const pdfViewerUpdateEnabled =
        document.documentElement.hasAttribute('pdf-viewer-update-enabled');
    const scrollContainer = pdfViewerUpdateEnabled ?
        /** @type {!HTMLElement} */ (this.getSizer().offsetParent) :
        document.documentElement;

    // Create the viewport.
    const defaultZoom =
        this.browserApi.getZoomBehavior() === ZoomBehavior.MANAGE ?
        this.browserApi.getDefaultZoom() :
        1.0;

    this.viewport_ = new Viewport(
        scrollContainer, this.getSizer(), this.getContent(),
        getScrollbarWidth(), defaultZoom, this.getToolbarHeight(),
        this.hasFixedToolbar());
    this.viewport_.setViewportChangedCallback(() => this.viewportChanged_());
    this.viewport_.setBeforeZoomCallback(
        () => this.currentController.beforeZoom());
    this.viewport_.setAfterZoomCallback(() => {
      this.currentController.afterZoom();
      this.afterZoom(this.viewport_.getZoom());
    });
    this.viewport_.setUserInitiatedCallback(
        userInitiated => this.setUserInitiated_(userInitiated));
    window.addEventListener('beforeunload', () => this.resetTrackers_());

    // Handle scripting messages from outside the extension that wish to
    // interact with it. We also send a message indicating that extension has
    // loaded and is ready to receive messages.
    window.addEventListener('message', message => {
      this.handleScriptingMessage(/** @type {!MessageObject} */ (message));
    }, false);

    // Create the plugin.
    this.plugin_ = this.createPlugin_(pdfViewerUpdateEnabled);
    this.getContent().appendChild(this.plugin_);
    this.pluginController_ = new PluginController(
        this.plugin_, this.viewport_, () => this.isUserInitiatedEvent,
        () => this.loaded);
    this.currentController = this.pluginController_;
    this.tracker.add(
        this.pluginController_.getEventTarget(), 'plugin-message',
        e => this.handlePluginMessage(e));

    document.body.addEventListener('change-page-and-xy', e => {
      const point = this.viewport_.convertPageToScreen(e.detail.page, e.detail);
      this.viewport_.goToPageAndXY(e.detail.page, point.x, point.y);
    });

    // Set up the ZoomManager.
    this.zoomManager_ = ZoomManager.create(
        this.browserApi.getZoomBehavior(), () => this.viewport_.getZoom(),
        zoom => this.browserApi.setZoom(zoom),
        this.browserApi.getInitialZoom());
    this.viewport_.setZoomManager(assert(this.zoomManager_));
    this.browserApi.addZoomEventListener(
        zoom => this.zoomManager_.onBrowserZoomChange(zoom));

    this.viewportScroller =
        new ViewportScroller(this.viewport_, this.plugin_, window);

    // Request translated strings.
    chrome.resourcesPrivate.getStrings(
        chrome.resourcesPrivate.Component.PDF,
        strings => this.handleStrings(strings));
  }

  /**
   * Update the loading progress of the document in response to a progress
   * message being received from the content controller.
   * @param {number} progress the progress as a percentage.
   */
  updateProgress(progress) {
    if (progress === -1) {
      // Document load failed.
      this.getErrorScreen().show();
      this.getSizer().style.display = 'none';
      this.setLoadState(LoadState.FAILED);
      this.sendDocumentLoadedMessage();
    } else if (progress === 100) {
      // Document load complete.
      if (this.lastViewportPosition) {
        this.viewport_.position = this.lastViewportPosition;
      }
      this.paramsParser.getViewportFromUrlParams(
          this.originalUrl, params => this.handleURLParams_(params));
      this.setLoadState(LoadState.SUCCESS);
      this.sendDocumentLoadedMessage();
      while (this.delayedScriptingMessages_.length > 0) {
        this.handleScriptingMessage(this.delayedScriptingMessages_.shift());
      }
    } else {
      this.setLoadState(LoadState.LOADING);
    }
  }

  /** @return {boolean} Whether the documentLoaded message can be sent. */
  readyToSendLoadMessage() {
    return true;
  }

  /**
   * Sends a 'documentLoaded' message to the PDFScriptingAPI if the document has
   * finished loading.
   */
  sendDocumentLoadedMessage() {
    if (this.loadState_ === LoadState.LOADING ||
        !this.readyToSendLoadMessage()) {
      return;
    }
    this.sendScriptingMessage(
        {type: 'documentLoaded', load_state: this.loadState_});
  }

  /**
   * Called to update the UI before sending the viewport scripting message.
   * Should be overridden by subclasses.
   * @protected
   */
  updateUIForViewportChange() {}

  /**
   * A callback that's called after the viewport changes.
   * @private
   */
  viewportChanged_() {
    if (!this.documentDimensions) {
      return;
    }

    this.updateUIForViewportChange();

    const visiblePage = this.viewport_.getMostVisiblePage();
    const visiblePageDimensions = this.viewport_.getPageScreenRect(visiblePage);
    const size = this.viewport_.size;
    this.sendScriptingMessage({
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
    if (this.parentWindow_ !== message.source) {
      this.parentWindow_ = message.source;
      this.parentOrigin_ = message.origin;
      // Ensure that we notify the embedder if the document is loaded.
      if (this.loadState_ !== LoadState.LOADING) {
        this.sendDocumentLoadedMessage();
      }
    }
  }

  /**
   * @param {!MessageObject} message The message to handle.
   * @return {boolean} Whether the message was delayed and added to the queue.
   */
  delayScriptingMessage(message) {
    // Delay scripting messages from users of the scripting API until the
    // document is loaded. This simplifies use of the APIs.
    if (this.loadState_ !== LoadState.SUCCESS) {
      this.delayedScriptingMessages_.push(message);
      return true;
    }
    return false;
  }

  /**
   * @param {!CustomEvent<MessageData>} e
   * @protected
   */
  handlePluginMessage(e) {}

  /**
   * Sets document dimensions from the current controller.
   * @param {!DocumentDimensionsMessageData} documentDimensions
   * @protected
   */
  setDocumentDimensions(documentDimensions) {
    this.documentDimensions = documentDimensions;
    this.isUserInitiatedEvent = false;
    this.viewport_.setDocumentDimensions(this.documentDimensions);
    this.isUserInitiatedEvent = true;
  }

  /**
   * @return {?Promise} Resolved when the load state reaches LOADED,
   *     rejects on FAILED. Returns null if no promise has been created, which
   *     is the case for initial load of the PDF.
   */
  get loaded() {
    return this.loaded_ ? this.loaded_.promise : null;
  }

  /** @return {!Viewport} */
  get viewport() {
    return assert(this.viewport_);
  }

  /**
   * @return {!PluginController}
   * @protected
   */
  get pluginController() {
    return assert(this.pluginController_);
  }

  /**
   * Updates the load state and triggers completion of the `loaded`
   * promise if necessary.
   * @param {!LoadState} loadState
   * @protected
   */
  setLoadState(loadState) {
    if (this.loadState_ === loadState) {
      return;
    }
    assert(
        loadState === LoadState.LOADING ||
        this.loadState_ === LoadState.LOADING);
    this.loadState_ = loadState;
    if (!this.initialLoadComplete_) {
      this.initialLoadComplete_ = true;
      return;
    }
    if (loadState === LoadState.SUCCESS) {
      this.loaded_.resolve();
    } else if (loadState === LoadState.FAILED) {
      this.loaded_.reject();
    } else {
      this.loaded_ = new PromiseResolver();
    }
  }

  /**
   * Load a dictionary of translated strings into the UI. Used as a callback for
   * chrome.resourcesPrivate.
   * @param {?Object} strings Dictionary of translated strings
   * @protected
   */
  handleStrings(strings) {
    if (!strings) {
      return;
    }
    loadTimeData.data = strings;

    // Predefined zoom factors to be used when zooming in/out. These are in
    // ascending order.
    const presetZoomFactors = /** @type {!Array<number>} */ (
        JSON.parse(loadTimeData.getString('presetZoomFactors')));
    this.viewport_.setZoomFactorRange(presetZoomFactors);

    this.strings = strings;
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
      this.isUserInitiatedEvent = false;
      this.updateViewportFit(params.view);
      this.forceFit(params.view);
      if (params.viewPosition) {
        const zoomedPositionShift =
            params.viewPosition * this.viewport_.getZoom();
        const currentViewportPosition = this.viewport_.position;
        if (params.view === FittingType.FIT_TO_WIDTH) {
          currentViewportPosition.y += zoomedPositionShift;
        } else if (params.view === FittingType.FIT_TO_HEIGHT) {
          currentViewportPosition.x += zoomedPositionShift;
        }
        this.viewport_.position = currentViewportPosition;
      }
      this.isUserInitiatedEvent = true;
    }
  }

  /**
   * A callback that sets |isUserInitiatedEvent| to |userInitiated|.
   * @param {boolean} userInitiated The value to set |isUserInitiatedEvent| to.
   * @private
   */
  setUserInitiated_(userInitiated) {
    assert(this.isUserInitiatedEvent !== userInitiated);
    this.isUserInitiatedEvent = userInitiated;
  }

  overrideSendScriptingMessageForTest() {
    this.overrideSendScriptingMessageForTest_ = true;
  }

  /**
   * Send a scripting message outside the extension (typically to
   * PDFScriptingAPI in a page containing the extension).
   * @param {Object} message the message to send.
   * @protected
   */
  sendScriptingMessage(message) {
    if (this.parentWindow_ && this.parentOrigin_) {
      let targetOrigin;
      // Only send data back to the embedder if it is from the same origin,
      // unless we're sending it to ourselves (which could happen in the case
      // of tests). We also allow documentLoaded messages through as this won't
      // leak important information.
      if (this.parentOrigin_ === window.location.origin) {
        targetOrigin = this.parentOrigin_;
      } else if (message.type === 'documentLoaded') {
        targetOrigin = '*';
      } else {
        targetOrigin = this.originalUrl;
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
   * @param {!FittingType} fittingType
   * @protected
   */
  updateViewportFit(fittingType) {
    if (fittingType === FittingType.FIT_TO_PAGE) {
      this.viewport_.fitToPage();
    } else if (fittingType === FittingType.FIT_TO_WIDTH) {
      this.viewport_.fitToWidth();
    } else if (fittingType === FittingType.FIT_TO_HEIGHT) {
      this.viewport_.fitToHeight();
    }
  }

  /**
   * Request to change the viewport fitting type.
   * @param {!CustomEvent<!FittingType>} e
   * @protected
   */
  onFitToChanged(e) {
    this.updateViewportFit(e.detail);
    PDFMetrics.recordFitTo(e.detail);
  }

  /** @protected */
  onZoomIn() {
    this.viewport_.zoomIn();
    PDFMetrics.recordZoomAction(/*isZoomIn=*/ true);
  }

  /**
   * @param {!CustomEvent<number>} e
   * @protected
   */
  onZoomChanged(e) {
    this.viewport_.setZoom(e.detail / 100);
    PDFMetrics.record(UserAction.ZOOM_CUSTOM);
  }

  /** @protected */
  onZoomOut() {
    this.viewport_.zoomOut();
    PDFMetrics.recordZoomAction(/*isZoomIn=*/ false);
  }

  /**
   * Handles a selected text reply from the current controller.
   * @param {!Object} message
   * @protected
   */
  handleSelectedTextReply(message) {
    if (this.overrideSendScriptingMessageForTest_) {
      this.overrideSendScriptingMessageForTest_ = false;
      try {
        this.sendScriptingMessage(message);
      } finally {
        this.parentWindow_.postMessage('flush', '*');
      }
      return;
    }
    this.sendScriptingMessage(message);
  }

  /** @protected */
  rotateClockwise() {
    PDFMetrics.record(UserAction.ROTATE);
    this.viewport_.rotateClockwise();
    this.currentController.rotateClockwise();
  }

  /** @protected */
  rotateCounterclockwise() {
    PDFMetrics.record(UserAction.ROTATE);
    this.viewport_.rotateCounterclockwise();
    this.currentController.rotateCounterclockwise();
  }

  /** @private */
  resetTrackers_() {
    this.viewport_.resetTracker();
    if (this.tracker) {
      this.tracker.removeAll();
    }
  }
}

customElements.define(PDFViewerBaseElement.is, PDFViewerBaseElement);
