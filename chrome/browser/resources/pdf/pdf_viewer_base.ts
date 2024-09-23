// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {BrowserApi} from './browser_api.js';
import {ZoomBehavior} from './browser_api.js';
import type {Point} from './constants.js';
import {FittingType} from './constants.js';
import type {ContentController, MessageData} from './controller.js';
import {PluginController, PluginControllerEventType} from './controller.js';
import {record, recordFitTo, UserAction} from './metrics.js';
import type {OpenPdfParams} from './open_pdf_params_parser.js';
import {OpenPdfParamsParser} from './open_pdf_params_parser.js';
import type {SerializedKeyEvent} from './pdf_scripting_api.js';
import {LoadState} from './pdf_scripting_api.js';
import type {DocumentDimensionsMessageData} from './pdf_viewer_utils.js';
import {Viewport} from './viewport.js';
import {ZoomManager} from './zoom_manager.js';

/** @return Width of a scrollbar in pixels */
function getScrollbarWidth(): number {
  const div = document.createElement('div');
  div.style.visibility = 'hidden';
  div.style.overflow = 'scroll';
  div.style.width = '50px';
  div.style.height = '50px';
  div.style.position = 'absolute';
  document.body.appendChild(div);
  const result = div.offsetWidth - div.clientWidth;
  div.parentNode!.removeChild(div);
  return result;
}

export type KeyEventData = MessageData&{keyEvent: SerializedKeyEvent};

export abstract class PdfViewerBaseElement extends CrLitElement {
  static override get properties() {
    return {
      pdfCr23Enabled: {type: Boolean},
      showErrorDialog: {type: Boolean},
      strings: {type: Object},
    };
  }

  protected browserApi: BrowserApi|null = null;
  protected currentController: ContentController|null = null;
  protected documentDimensions: DocumentDimensionsMessageData|null = null;
  protected isUserInitiatedEvent: boolean = true;
  protected lastViewportPosition: Point|null = null;
  protected originalUrl: string = '';
  protected paramsParser: OpenPdfParamsParser|null = null;
  protected pdfCr23Enabled: boolean = false;
  protected pdfOopifEnabled: boolean = false;
  showErrorDialog: boolean = false;
  protected strings?: {[key: string]: string};
  protected tracker: EventTracker = new EventTracker();
  private delayedScriptingMessages_: MessageEvent[] = [];
  private initialLoadComplete_: boolean = false;
  private loaded_: PromiseResolver<void>|null = null;
  private loadState_: LoadState = LoadState.LOADING;
  private overrideSendScriptingMessageForTest_: boolean = false;
  private parentOrigin_: string|null = null;
  private parentWindow_: WindowProxy|null = null;
  private plugin_: HTMLEmbedElement|null = null;
  private viewport_: Viewport|null = null;
  private zoomManager_: ZoomManager|null = null;

  protected abstract forceFit(view: FittingType): void;

  protected abstract afterZoom(viewportZoom: number): void;

  protected abstract setPluginSrc(plugin: HTMLEmbedElement): void;

  /** Whether to enable the new UI. */
  protected isNewUiEnabled(): boolean {
    return true;
  }

  abstract getBackgroundColor(): number;

  /** Creates the plugin element. */
  private createPlugin_(): HTMLEmbedElement {
    // Create the plugin object dynamically. The plugin element is sized to
    // fill the entire window and is set to be fixed positioning, acting as a
    // viewport. The plugin renders into this viewport according to the scroll
    // position of the window.
    const plugin = document.createElement('embed');

    // NOTE: The plugin's 'id' field must be set to 'plugin' since
    // ChromePrintRenderFrameHelperDeleage::GetPdfElement() in
    // chrome/renderer/printing/chrome_print_render_frame_helper_delegate.cc
    // actually references it.
    plugin.id = 'plugin';
    plugin.type = 'application/x-google-chrome-pdf';

    plugin.setAttribute('original-url', this.originalUrl);
    this.setPluginSrc(plugin);

    plugin.setAttribute(
        'background-color', this.getBackgroundColor().toString());

    const javascript = this.browserApi!.getStreamInfo().javascript || 'block';
    plugin.setAttribute('javascript', javascript);

    if (this.browserApi!.getStreamInfo().embedded) {
      plugin.setAttribute(
          'top-level-url', this.browserApi!.getStreamInfo().tabUrl!);
    } else {
      plugin.toggleAttribute('full-frame', true);
    }

    if (this.isNewUiEnabled()) {
      plugin.toggleAttribute('pdf-viewer-update-enabled', true);
    }

    // Pass the attributes for loading PDF plugin through the `pdfViewerPrivate`
    // API if OOPIF PDF is enabled, or the `mimeHandlerPrivate` API.
    const attributesForLoading:
        chrome.mimeHandlerPrivate.PdfPluginAttributes = {
      backgroundColor: this.getBackgroundColor(),
      allowJavascript: javascript === 'allow',
    };

    // PDF viewer only, as Print Preview doesn't set PDF plugin attributes.
    if (this.pdfOopifEnabled) {
      if (chrome.pdfViewerPrivate) {
        chrome.pdfViewerPrivate.setPdfPluginAttributes(attributesForLoading);
      }
    } else if (chrome.mimeHandlerPrivate) {
      chrome.mimeHandlerPrivate.setPdfPluginAttributes(attributesForLoading);
    }

    return plugin;
  }

  abstract init(browserApi: BrowserApi): void;

  /**
   * Initializes the PDF viewer.
   * @param browserApi The interface with the browser.
   * @param scroller The viewport's scroller element.
   * @param sizer The viewport's sizer element.
   * @param content The viewport's content element.
   */
  protected initInternal(
      browserApi: BrowserApi, scroller: HTMLElement, sizer: HTMLElement,
      content: HTMLElement) {
    this.browserApi = browserApi;
    this.originalUrl = this.browserApi!.getStreamInfo().originalUrl;
    this.pdfCr23Enabled =
        document.documentElement.hasAttribute('pdfCr23Enabled');
    this.pdfOopifEnabled =
        document.documentElement.hasAttribute('pdfOopifEnabled');

    record(UserAction.DOCUMENT_OPENED);

    // Create the viewport.
    const defaultZoom =
        this.browserApi!.getZoomBehavior() === ZoomBehavior.MANAGE ?
        this.browserApi!.getDefaultZoom() :
        1.0;

    this.viewport_ = new Viewport(
        scroller, sizer, content, getScrollbarWidth(), defaultZoom);
    this.viewport_!.setViewportChangedCallback(() => this.viewportChanged_());
    this.viewport_!.setBeforeZoomCallback(
        () => this.currentController!.beforeZoom());
    this.viewport_!.setAfterZoomCallback(() => {
      this.currentController!.afterZoom();
      this.afterZoom(this.viewport_!.getZoom());
    });
    this.viewport_!.setUserInitiatedCallback(
        userInitiated => this.setUserInitiated_(userInitiated));
    window.addEventListener('beforeunload', (event: BeforeUnloadEvent) =>
        this.onBeforeUnload(event),
    );

    // Handle scripting messages from outside the extension that wish to
    // interact with it. We also send a message indicating that extension has
    // loaded and is ready to receive messages.
    window.addEventListener('message', message => {
      this.handleScriptingMessage(message);
    }, false);

    // Create the plugin.
    this.plugin_ = this.createPlugin_();

    const pluginController = PluginController.getInstance();
    pluginController.init(
        this.plugin_, this.viewport_, () => this.isUserInitiatedEvent,
        () => this.loaded);
    pluginController.isActive = true;
    this.currentController = pluginController;

    // Parse open pdf parameters.
    const getNamedDestinationCallback = (destination: string) => {
      return PluginController.getInstance().getNamedDestination(destination);
    };
    const getPageBoundingBoxCallback = (page: number) => {
      return PluginController.getInstance().getPageBoundingBox(page);
    };
    this.paramsParser = new OpenPdfParamsParser(
        getNamedDestinationCallback, getPageBoundingBoxCallback);

    this.tracker.add(
        pluginController.getEventTarget(),
        PluginControllerEventType.PLUGIN_MESSAGE,
        (e: Event) => this.handlePluginMessage(e as CustomEvent<MessageData>));

    document.body.addEventListener('change-page-and-xy', e => {
      const point =
          this.viewport_!.convertPageToScreen(e.detail.page, e.detail);
      this.viewport_!.goToPageAndXy(e.detail.page, point.x, point.y);
    });

    // Setup the keyboard event listener.
    document.addEventListener('keydown', this.handleKeyEvent.bind(this));

    // Set up the ZoomManager.
    this.zoomManager_ = ZoomManager.create(
        this.browserApi!.getZoomBehavior(), () => this.viewport_!.getZoom(),
        zoom => this.browserApi!.setZoom(zoom),
        this.browserApi!.getInitialZoom());
    this.viewport_!.setZoomManager(this.zoomManager_);
    this.browserApi!.addZoomEventListener(
        (zoom: number) => this.zoomManager_!.onBrowserZoomChange(zoom));

    // Request translated strings.
    chrome.resourcesPrivate.getStrings(
        chrome.resourcesPrivate.Component.PDF,
        strings => this.handleStrings(strings));
  }

  /**
   * Updates the loading progress of the document in response to a progress
   * message being received from the content controller.
   * @param progress The progress as a percentage.
   */
  updateProgress(progress: number) {
    if (progress === -1) {
      // Document load failed.
      this.showErrorDialog = true;
      this.viewport_!.setContent(null);
      this.setLoadState(LoadState.FAILED);
      this.sendDocumentLoadedMessage();
    } else if (progress === 100) {
      // Document load complete.
      if (this.lastViewportPosition) {
        this.viewport_!.setPosition(this.lastViewportPosition);
      }
      this.paramsParser!.getViewportFromUrlParams(this.originalUrl)
          .then(params => this.handleUrlParams_(params));
      this.setLoadState(LoadState.SUCCESS);
      this.sendDocumentLoadedMessage();
      while (this.delayedScriptingMessages_.length > 0) {
        this.handleScriptingMessage(this.delayedScriptingMessages_.shift()!);
      }
    } else {
      this.setLoadState(LoadState.LOADING);
    }
  }

  /** @return Whether the documentLoaded message can be sent. */
  readyToSendLoadMessage(): boolean {
    return true;
  }

  /**
   * Sends a 'documentLoaded' message to the PdfScriptingApi if the document has
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

  /** Updates the UI before sending the viewport scripting message. */
  protected abstract updateUiForViewportChange(): void;

  /** A callback to be called after the viewport changes. */
  private viewportChanged_() {
    if (!this.documentDimensions) {
      return;
    }

    this.updateUiForViewportChange();

    const visiblePage = this.viewport_!.getMostVisiblePage();
    const visiblePageDimensions =
        this.viewport_!.getPageScreenRect(visiblePage);
    const size = this.viewport_!.size;
    this.paramsParser!.setViewportDimensions(size);

    this.sendScriptingMessage({
      type: 'viewport',
      pageX: visiblePageDimensions.x,
      pageY: visiblePageDimensions.y,
      pageWidth: visiblePageDimensions.width,
      viewportWidth: size.width,
      viewportHeight: size.height,
    });
  }

  /**
   * Handles a scripting message from outside the extension (typically sent by
   * PdfScriptingApi in a page containing the extension) to interact with the
   * plugin.
   * @return Whether the message was handled.
   */
  handleScriptingMessage(message: MessageEvent): boolean {
    // TODO(crbug.com/40189769): Remove this message handler when a permanent
    // postMessage() bridge is implemented for the viewer.
    if (message.data.type === 'connect') {
      const token: string = message.data.token;
      if (token === this.browserApi!.getStreamInfo().streamUrl) {
        assert(message.ports[0] !== undefined);
        PluginController.getInstance().bindMessageHandler(message.ports[0]);
      } else {
        this.dispatchEvent(new CustomEvent('connection-denied-for-testing'));
      }
      return true;
    }

    if (this.parentWindow_ !== message.source) {
      this.parentWindow_ = message.source as WindowProxy;
      this.parentOrigin_ = message.origin;
      // Ensure that we notify the embedder if the document is loaded.
      if (this.loadState_ !== LoadState.LOADING) {
        this.sendDocumentLoadedMessage();
      }
    }
    return false;
  }

  /**
   * @return Whether the message was delayed and added to the queue.
   */
  delayScriptingMessage(message: MessageEvent): boolean {
    // Delay scripting messages from users of the scripting API until the
    // document is loaded. This simplifies use of the APIs.
    if (this.loadState_ !== LoadState.SUCCESS) {
      this.delayedScriptingMessages_.push(message);
      return true;
    }
    return false;
  }

  protected abstract handlePluginMessage(e: CustomEvent<MessageData>): void;

  /**
   * Handles key events. For instance, these may come from the user directly,
   * the plugin frame, or the scripting API.
   */
  protected abstract handleKeyEvent(e: KeyboardEvent): void;

  /** Sets document dimensions from the current controller. */
  protected setDocumentDimensions(documentDimensions:
                                      DocumentDimensionsMessageData) {
    this.documentDimensions = documentDimensions;
    this.isUserInitiatedEvent = false;
    this.viewport_!.setDocumentDimensions(this.documentDimensions);
    this.paramsParser!.setPageCount(documentDimensions.pageDimensions.length);
    this.paramsParser!.setViewportDimensions(this.viewport_!.size);
    this.isUserInitiatedEvent = true;
  }

  /**
   * @return True if OOPIF PDF is enabled, false otherwise.
   */
  get isPdfOopifEnabled(): boolean {
    return this.pdfOopifEnabled;
  }

  /**
   * @return Resolved when the load state reaches LOADED, rejects on FAILED.
   *     Returns null if no promise has been created, which is the case for
   *     initial load of the PDF.
   */
  get loaded(): Promise<void>|null {
    return this.loaded_ ? this.loaded_!.promise : null;
  }

  get viewport(): Viewport {
    assert(this.viewport_);
    return this.viewport_;
  }

  /**
   * Updates the load state and triggers completion of the `loaded`
   * promise if necessary.
   */
  protected setLoadState(loadState: LoadState) {
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
      this.loaded_!.resolve();
    } else if (loadState === LoadState.FAILED) {
      this.loaded_!.reject();
    } else {
      this.loaded_ = new PromiseResolver();
    }
  }

  /**
   * Load a dictionary of translated strings into the UI. Used as a callback for
   * chrome.resourcesPrivate.
   * @param strings Dictionary of translated strings
   */
  protected handleStrings(strings?: {[key: string]: string}) {
    if (!strings) {
      return;
    }
    loadTimeData.data = strings;

    // Predefined zoom factors to be used when zooming in/out. These are in
    // ascending order.
    const presetZoomFactors =
        JSON.parse(loadTimeData.getString('presetZoomFactors')) as number[];
    this.viewport_!.setZoomFactorRange(presetZoomFactors);

    this.strings = strings;
  }

  /**
   * Handles open pdf parameters. This function updates the viewport as per the
   * parameters appended to the URL when opening pdf. The order is important as
   * later actions can override the effects of previous actions.
   * @param params The open params passed in the URL.
   */
  private handleUrlParams_(params: OpenPdfParams) {
    assert(this.viewport_);

    if (params.zoom) {
      this.viewport_.setZoom(params.zoom);
    }

    if (params.position) {
      this.viewport_.goToPageAndXy(
          params.page || 0, params.position.x, params.position.y);
    }

    if (params.view) {
      this.isUserInitiatedEvent = false;
      const fittingTypeParams = {
        boundingBox: params.boundingBox,
        page: params.page || 0,
        viewPosition: params.viewPosition,
        fitToWidth: params.view === FittingType.FIT_TO_BOUNDING_BOX_WIDTH,
      };
      this.viewport_.setFittingType(params.view, fittingTypeParams);
      this.forceFit(params.view);
      this.isUserInitiatedEvent = true;
    } else if (!params.position && params.page) {
      // No fitting type provided, so just go to page.
      this.viewport_.goToPage(params.page);
    }
  }

  /**
   * A callback that sets `isUserInitiatedEvent` to `userInitiated`.
   * @param userInitiated The value to which to set `isUserInitiatedEvent`.
   */
  private setUserInitiated_(userInitiated: boolean) {
    assert(this.isUserInitiatedEvent !== userInitiated);
    this.isUserInitiatedEvent = userInitiated;
  }

  overrideSendScriptingMessageForTest() {
    this.overrideSendScriptingMessageForTest_ = true;
  }

  /**
   * Send a scripting message outside the extension (typically to
   * PdfScriptingApi in a page containing the extension).
   */
  protected sendScriptingMessage(message: any) {
    if (this.parentWindow_ && this.parentOrigin_) {
      let targetOrigin;
      // Only send data back to the embedder if it is from the same origin,
      // unless we're sending it to ourselves (which could happen in the case
      // of tests). We also allow 'documentLoaded' and 'passwordPrompted'
      // messages through as they do not leak sensitive information.
      if (this.parentOrigin_ === window.location.origin) {
        targetOrigin = this.parentOrigin_;
      } else if (
          message.type === 'documentLoaded' ||
          message.type === 'passwordPrompted') {
        targetOrigin = '*';
      } else {
        targetOrigin = this.originalUrl;
      }
      try {
        this.parentWindow_!.postMessage(message, targetOrigin);
      } catch (ok) {
        // TODO(crbug.com/40647731): targetOrigin probably was rejected, such as
        // a "data:" URL. This shouldn't cause this method to throw, though.
      }
    }
  }

  /** Requests to change the viewport fitting type. */
  protected onFitToChanged(e: CustomEvent<FittingType>) {
    this.viewport_!.setFittingType(e.detail);
    recordFitTo(e.detail);
  }

  protected onZoomIn() {
    this.viewport_!.zoomIn();
    record(UserAction.ZOOM_IN);
  }

  protected onZoomChanged(e: CustomEvent<number>) {
    this.viewport_!.setZoom(e.detail / 100);
    record(UserAction.ZOOM_CUSTOM);
  }

  protected onZoomOut() {
    this.viewport_!.zoomOut();
    record(UserAction.ZOOM_OUT);
  }

  /** Handles a selected text reply from the current controller. */
  protected handleSelectedTextReply(message: {selectedText: string}) {
    if (this.overrideSendScriptingMessageForTest_) {
      this.overrideSendScriptingMessageForTest_ = false;
      try {
        this.sendScriptingMessage(message);
      } finally {
        this.parentWindow_!.postMessage('flush', '*');
      }
      return;
    }
    this.sendScriptingMessage(message);
  }

  protected rotateClockwise() {
    record(UserAction.ROTATE);
    this.currentController!.rotateClockwise();
  }

  protected rotateCounterclockwise() {
    record(UserAction.ROTATE);
    this.currentController!.rotateCounterclockwise();
  }

  /**
   * Handles the `BeforeUnloadEvent` event.
   * @param event The `BeforeUnloadEvent` object representing the event.
   */
  protected onBeforeUnload(_: BeforeUnloadEvent) {
    this.resetTrackers_();
  }

  private resetTrackers_() {
    this.viewport_!.resetTracker();
    if (this.tracker) {
      this.tracker.removeAll();
    }
  }
}
