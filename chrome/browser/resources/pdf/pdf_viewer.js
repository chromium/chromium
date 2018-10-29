// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @typedef {{
 *   dataToSave: Array,
 *   token: string,
 *   fileName: string
 * }}
 */
let SaveDataMessageData;

/**
 * @return {number} Width of a scrollbar in pixels
 */
function getScrollbarWidth() {
  var div = document.createElement('div');
  div.style.visibility = 'hidden';
  div.style.overflow = 'scroll';
  div.style.width = '50px';
  div.style.height = '50px';
  div.style.position = 'absolute';
  document.body.appendChild(div);
  var result = div.offsetWidth - div.clientWidth;
  div.parentNode.removeChild(div);
  return result;
}

/**
 * Return the filename component of a URL, percent decoded if possible.
 *
 * @param {string} url The URL to get the filename from.
 * @return {string} The filename component.
 */
function getFilenameFromURL(url) {
  // Ignore the query and fragment.
  var mainUrl = url.split(/#|\?/)[0];
  var components = mainUrl.split(/\/|\\/);
  var filename = components[components.length - 1];
  try {
    return decodeURIComponent(filename);
  } catch (e) {
    if (e instanceof URIError)
      return filename;
    throw e;
  }
}

/**
 * Whether keydown events should currently be ignored. Events are ignored when
 * an editable element has focus, to allow for proper editing controls.
 *
 * @param {HTMLElement} activeElement The currently selected DOM node.
 * @return {boolean} True if keydown events should be ignored.
 */
function shouldIgnoreKeyEvents(activeElement) {
  while (activeElement.shadowRoot != null &&
         activeElement.shadowRoot.activeElement != null) {
    activeElement = activeElement.shadowRoot.activeElement;
  }

  return (
      activeElement.isContentEditable || activeElement.tagName == 'INPUT' ||
      activeElement.tagName == 'TEXTAREA');
}

/**
 * Creates a cryptographically secure pseudorandom 128-bit token.
 *
 * @return {string} The generated token as a hex string.
 */
function createToken() {
  const randomBytes = new Uint8Array(16);
  return window.crypto.getRandomValues(randomBytes)
      .map(b => b.toString(16).padStart(2, '0'))
      .join('');
}

/**
 * The minimum number of pixels to offset the toolbar by from the bottom and
 * right side of the screen.
 */
PDFViewer.MIN_TOOLBAR_OFFSET = 15;

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
 * The light-gray background color used for print preview.
 */
PDFViewer.LIGHT_BACKGROUND_COLOR = '0xFFCCCCCC';

/**
 * The dark-gray background color used for the regular viewer.
 */
PDFViewer.DARK_BACKGROUND_COLOR = '0xFF525659';

/**
 * Creates a new PDFViewer. There should only be one of these objects per
 * document.
 *
 * @param {!BrowserApi} browserApi An object providing an API to the browser.
 * @constructor
 */
function PDFViewer(browserApi) {
  this.browserApi_ = browserApi;
  this.originalUrl_ = this.browserApi_.getStreamInfo().originalUrl;
  this.javascript_ = this.browserApi_.getStreamInfo().javascript || 'block';
  this.loadState_ = LoadState.LOADING;
  this.parentWindow_ = null;
  this.parentOrigin_ = null;
  this.isFormFieldFocused_ = false;
  this.beepCount_ = 0;
  this.delayedScriptingMessages_ = [];

  /** @private {!Set<string>} */
  this.pendingTokens_ = new Set();

  this.isPrintPreview_ = location.origin === 'chrome://print';
  this.isPrintPreviewLoadingFinished_ = false;
  this.isUserInitiatedEvent_ = true;

  /**
   * @type {!PDFMetrics}
   */
  this.metrics =
      (chrome.metricsPrivate ? new PDFMetricsImpl() : new PDFMetricsDummy());
  this.metrics.onDocumentOpened();

  /**
   * @private {!PDFCoordsTransformer}
   */
  this.coordsTransformer_ =
      new PDFCoordsTransformer(this.postMessage_.bind(this));

  // Parse open pdf parameters.
  this.paramsParser_ = new OpenPDFParamsParser(this.postMessage_.bind(this));
  var toolbarEnabled =
      this.paramsParser_.getUiUrlParams(this.originalUrl_).toolbar &&
      !this.isPrintPreview_;

  // The sizer element is placed behind the plugin element to cause scrollbars
  // to be displayed in the window. It is sized according to the document size
  // of the pdf and zoom level.
  this.sizer_ = $('sizer');
  if (this.isPrintPreview_)
    this.pageIndicator_ = $('page-indicator');
  this.passwordScreen_ = $('password-screen');
  this.passwordScreen_.addEventListener(
      'password-submitted', this.onPasswordSubmitted_.bind(this));
  this.errorScreen_ = $('error-screen');
  // Can only reload if we are in a normal tab.
  if (chrome.tabs && this.browserApi_.getStreamInfo().tabId != -1) {
    this.errorScreen_.reloadFn = () => {
      chrome.tabs.reload(this.browserApi_.getStreamInfo().tabId);
    };
  }

  // Create the viewport.
  var shortWindow = window.innerHeight < PDFViewer.TOOLBAR_WINDOW_MIN_HEIGHT;
  var topToolbarHeight =
      (toolbarEnabled) ? PDFViewer.MATERIAL_TOOLBAR_HEIGHT : 0;
  var defaultZoom =
      this.browserApi_.getZoomBehavior() == BrowserApi.ZoomBehavior.MANAGE ?
      this.browserApi_.getDefaultZoom() :
      1.0;
  this.viewport_ = new Viewport(
      window, this.sizer_, this.viewportChanged_.bind(this),
      this.beforeZoom_.bind(this), this.afterZoom_.bind(this),
      this.setUserInitiated_.bind(this), getScrollbarWidth(), defaultZoom,
      topToolbarHeight);

  // Create the plugin object dynamically so we can set its src. The plugin
  // element is sized to fill the entire window and is set to be fixed
  // positioning, acting as a viewport. The plugin renders into this viewport
  // according to the scroll position of the window.
  this.plugin_ = document.createElement('embed');
  // NOTE: The plugin's 'id' field must be set to 'plugin' since
  // chrome/renderer/printing/print_render_frame_helper.cc actually
  // references it.
  this.plugin_.id = 'plugin';
  this.plugin_.type = 'application/x-google-chrome-pdf';
  this.plugin_.addEventListener(
      'message', this.handlePluginMessage_.bind(this), false);

  // Handle scripting messages from outside the extension that wish to interact
  // with it. We also send a message indicating that extension has loaded and
  // is ready to receive messages.
  window.addEventListener(
      'message', this.handleScriptingMessage.bind(this), false);

  this.plugin_.setAttribute('src', this.originalUrl_);
  this.plugin_.setAttribute(
      'stream-url', this.browserApi_.getStreamInfo().streamUrl);
  var headers = '';
  for (var header in this.browserApi_.getStreamInfo().responseHeaders) {
    headers += header + ': ' +
        this.browserApi_.getStreamInfo().responseHeaders[header] + '\n';
  }
  this.plugin_.setAttribute('headers', headers);

  var backgroundColor = PDFViewer.DARK_BACKGROUND_COLOR;
  this.plugin_.setAttribute('background-color', backgroundColor);
  this.plugin_.setAttribute('top-toolbar-height', topToolbarHeight);
  this.plugin_.setAttribute('javascript', this.javascript_);

  if (this.browserApi_.getStreamInfo().embedded) {
    this.plugin_.setAttribute(
        'top-level-url', this.browserApi_.getStreamInfo().tabUrl);
  } else {
    this.plugin_.setAttribute('full-frame', '');
  }
  document.body.appendChild(this.plugin_);

  // Setup the button event listeners.
  this.zoomToolbar_ = $('zoom-toolbar');
  this.zoomToolbar_.addEventListener(
      'fit-to-changed', this.fitToChanged_.bind(this));
  this.zoomToolbar_.addEventListener(
      'zoom-in', this.viewport_.zoomIn.bind(this.viewport_));
  this.zoomToolbar_.addEventListener(
      'zoom-out', this.viewport_.zoomOut.bind(this.viewport_));

  this.gestureDetector_ = new GestureDetector(this.plugin_);
  this.gestureDetector_.addEventListener(
      'pinchstart', this.onPinchStart_.bind(this));
  this.sentPinchEvent_ = false;
  this.gestureDetector_.addEventListener(
      'pinchupdate', this.onPinchUpdate_.bind(this));
  this.gestureDetector_.addEventListener(
      'pinchend', this.onPinchEnd_.bind(this));

  if (toolbarEnabled) {
    this.toolbar_ = $('toolbar');
    this.toolbar_.hidden = false;
    this.toolbar_.addEventListener('save', this.save_.bind(this));
    this.toolbar_.addEventListener('print', this.print_.bind(this));
    this.toolbar_.addEventListener(
        'rotate-right', this.rotateClockwise_.bind(this));
    // Must attach to mouseup on the plugin element, since it eats mousedown
    // and click events.
    this.plugin_.addEventListener(
        'mouseup', this.toolbar_.hideDropdowns.bind(this.toolbar_));

    this.toolbar_.docTitle = getFilenameFromURL(this.originalUrl_);
  }

  document.body.addEventListener('change-page', e => {
    this.viewport_.goToPage(e.detail.page);
    if (e.detail.origin == 'bookmark')
      this.metrics.onFollowBookmark();
    else if (e.detail.origin == 'pageselector')
      this.metrics.onPageSelectorNavigation();
  });

  document.body.addEventListener('change-page-and-xy', e => {
    // The coordinates received in |e| are in page coordinates and need to be
    // transformed to screen coordinates.
    this.coordsTransformer_.request(
        this.goToPageAndXY_.bind(this, e.detail.origin, e.detail.page), {},
        e.detail.page, e.detail.x, e.detail.y);
  });

  document.body.addEventListener('navigate', e => {
    var disposition = e.detail.newtab ?
        Navigator.WindowOpenDisposition.NEW_BACKGROUND_TAB :
        Navigator.WindowOpenDisposition.CURRENT_TAB;
    this.navigator_.navigate(e.detail.uri, disposition);
  });

  document.body.addEventListener('dropdown-opened', e => {
    if (e.detail == 'bookmarks')
      this.metrics.onOpenBookmarksPanel();
  });

  this.toolbarManager_ =
      new ToolbarManager(window, this.toolbar_, this.zoomToolbar_);

  // Set up the ZoomManager.
  this.zoomManager_ = ZoomManager.create(
      this.browserApi_.getZoomBehavior(), this.viewport_,
      this.browserApi_.setZoom.bind(this.browserApi_),
      this.browserApi_.getInitialZoom());
  this.viewport_.zoomManager = this.zoomManager_;
  this.browserApi_.addZoomEventListener(
      this.zoomManager_.onBrowserZoomChange.bind(this.zoomManager_));

  // Setup the keyboard event listener.
  document.addEventListener('keydown', this.handleKeyEvent_.bind(this));
  document.addEventListener('mousemove', this.handleMouseEvent_.bind(this));
  document.addEventListener('mouseout', this.handleMouseEvent_.bind(this));
  document.addEventListener(
      'contextmenu', this.handleContextMenuEvent_.bind(this));

  var tabId = this.browserApi_.getStreamInfo().tabId;
  this.navigator_ = new Navigator(
      this.originalUrl_, this.viewport_, this.paramsParser_,
      new NavigatorDelegate(tabId));
  this.viewportScroller_ =
      new ViewportScroller(this.viewport_, this.plugin_, window);

  // Request translated strings.
  chrome.resourcesPrivate.getStrings('pdf', this.handleStrings_.bind(this));
}

PDFViewer.prototype = {
  /**
   * Handle key events. These may come from the user directly or via the
   * scripting API.
   *
   * @param {KeyboardEvent} e the event to handle.
   * @private
   */
  handleKeyEvent_: function(e) {
    var position = this.viewport_.position;
    // Certain scroll events may be sent from outside of the extension.
    var fromScriptingAPI = e.fromScriptingAPI;

    if (shouldIgnoreKeyEvents(document.activeElement) || e.defaultPrevented)
      return;

    this.toolbarManager_.hideToolbarsAfterTimeout(e);

    var pageUpHandler = () => {
      // Go to the previous page if we are fit-to-page or fit-to-height.
      if (this.viewport_.isPagedMode()) {
        this.viewport_.goToPage(this.viewport_.getMostVisiblePage() - 1);
        // Since we do the movement of the page.
        e.preventDefault();
      } else if (fromScriptingAPI) {
        position.y -= this.viewport.size.height;
        this.viewport.position = position;
      }
    };
    var pageDownHandler = () => {
      // Go to the next page if we are fit-to-page or fit-to-height.
      if (this.viewport_.isPagedMode()) {
        this.viewport_.goToPage(this.viewport_.getMostVisiblePage() + 1);
        // Since we do the movement of the page.
        e.preventDefault();
      } else if (fromScriptingAPI) {
        position.y += this.viewport.size.height;
        this.viewport.position = position;
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
        if (e.shiftKey)
          pageUpHandler();
        else
          pageDownHandler();
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
            this.viewport_.goToPage(this.viewport_.getMostVisiblePage() - 1);
            // Since we do the movement of the page.
            e.preventDefault();
          } else if (fromScriptingAPI) {
            position.x -= Viewport.SCROLL_INCREMENT;
            this.viewport.position = position;
          }
        }
        return;
      case 38:  // Up arrow key.
        if (fromScriptingAPI) {
          position.y -= Viewport.SCROLL_INCREMENT;
          this.viewport.position = position;
        }
        return;
      case 39:  // Right arrow key.
        if (!hasKeyModifiers(e)) {
          // Go to the next page if there are no horizontal scrollbars and no
          // form field is focused.
          if (!(this.viewport_.documentHasScrollbars().horizontal ||
                this.isFormFieldFocused_)) {
            this.viewport_.goToPage(this.viewport_.getMostVisiblePage() + 1);
            // Since we do the movement of the page.
            e.preventDefault();
          } else if (fromScriptingAPI) {
            position.x += Viewport.SCROLL_INCREMENT;
            this.viewport.position = position;
          }
        }
        return;
      case 40:  // Down arrow key.
        if (fromScriptingAPI) {
          position.y += Viewport.SCROLL_INCREMENT;
          this.viewport.position = position;
        }
        return;
      case 65:  // 'a' key.
        if (e.ctrlKey || e.metaKey) {
          this.postMessage_({type: 'selectAll'});
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
        if (e.ctrlKey)
          this.rotateCounterClockwise_();
        return;
      case 220:  // Backslash key.
        if (e.ctrlKey)
          this.zoomToolbar_.fitToggleFromHotKey();
        return;
      case 221:  // Right bracket key.
        if (e.ctrlKey)
          this.rotateClockwise_();
        return;
    }

    // Give print preview a chance to handle the key event.
    if (!fromScriptingAPI && this.isPrintPreview_) {
      this.sendScriptingMessage_(
          {type: 'sendKeyEvent', keyEvent: SerializeKeyEvent(e)});
    } else {
      // Show toolbars as a fallback.
      if (!(e.shiftKey || e.ctrlKey || e.altKey))
        this.toolbarManager_.showToolbars();
    }
  },

  handleMouseEvent_: function(e) {
    if (e.type == 'mousemove')
      this.toolbarManager_.handleMouseMove(e);
    else if (e.type == 'mouseout')
      this.toolbarManager_.hideToolbarsForMouseOut();
  },

  handleContextMenuEvent_: function(e) {
    // Stop Chrome from popping up the context menu on long press. We need to
    // make sure the start event did not have 2 touches because we don't want
    // to block two finger tap opening the context menu. We check for
    // firesTouchEvents in order to not block the context menu on right click.
    if (e.sourceCapabilities.firesTouchEvents &&
        !this.gestureDetector_.wasTwoFingerTouch()) {
      e.preventDefault();
    }
  },

  /**
   * Rotate the plugin clockwise.
   *
   * @private
   */
  rotateClockwise_: function() {
    this.metrics.onRotation();
    this.postMessage_({type: 'rotateClockwise'});
  },

  /**
   * Rotate the plugin counter-clockwise.
   *
   * @private
   */
  rotateCounterClockwise_: function() {
    this.metrics.onRotation();
    this.postMessage_({type: 'rotateCounterclockwise'});
  },

  /**
   * Request to change the viewport fitting type.
   *
   * @param {CustomEvent} e Event received with the new FittingType as detail.
   * @private
   */
  fitToChanged_: function(e) {
    if (e.detail.fittingType == FittingType.FIT_TO_PAGE) {
      this.viewport_.fitToPage();
      this.toolbarManager_.forceHideTopToolbar();
    } else if (e.detail.fittingType == FittingType.FIT_TO_WIDTH) {
      this.viewport_.fitToWidth();
    } else if (e.detail.fittingType == FittingType.FIT_TO_HEIGHT) {
      this.viewport_.fitToHeight();
      this.toolbarManager_.forceHideTopToolbar();
    }

    if (e.detail.userInitiated)
      this.metrics.onFitTo(e.detail.fittingType);
  },

  /**
   * Notify the plugin to print.
   *
   * @private
   */
  print_: function() {
    this.postMessage_({type: 'print'});
  },

  /**
   * Notify the plugin to save.
   *
   * @private
   */
  save_: function() {
    const newToken = createToken();
    this.pendingTokens_.add(newToken);
    this.postMessage_({type: 'save', token: newToken});
  },

  /**
   * Sends a 'documentLoaded' message to the PDFScriptingAPI if the document has
   * finished loading.
   *
   * @private
   */
  sendDocumentLoadedMessage_: function() {
    if (this.loadState_ == LoadState.LOADING)
      return;
    if (this.isPrintPreview_ && !this.isPrintPreviewLoadingFinished_)
      return;
    this.sendScriptingMessage_(
        {type: 'documentLoaded', load_state: this.loadState_});
  },

  /**
   * Handle open pdf parameters. This function updates the viewport as per
   * the parameters mentioned in the url while opening pdf. The order is
   * important as later actions can override the effects of previous actions.
   *
   * @param {Object} params The open params passed in the URL.
   * @private
   */
  handleURLParams_: function(params) {
    if (params.zoom)
      this.viewport_.setZoom(params.zoom);

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
        var zoomedPositionShift = params.viewPosition * this.viewport_.zoom;
        var currentViewportPosition = this.viewport_.position;
        if (params.view == FittingType.FIT_TO_WIDTH)
          currentViewportPosition.y += zoomedPositionShift;
        else if (params.view == FittingType.FIT_TO_HEIGHT)
          currentViewportPosition.x += zoomedPositionShift;
        this.viewport_.position = currentViewportPosition;
      }
      this.isUserInitiatedEvent_ = true;
    }
  },

  /**
   * Moves the viewport to a point in a page. Called back after a
   * 'transformPagePointReply' is returned from the plugin.
   *
   * @param {string} origin Identifier for the caller for logging purposes.
   * @param {number} page The index of the page to go to. zero-based.
   * @param {Object} message Message received from the plugin containing the
   *     x and y to navigate to in screen coordinates.
   * @private
   */
  goToPageAndXY_: function(origin, page, message) {
    this.viewport_.goToPageAndXY(page, message.x, message.y);
    if (origin == 'bookmark')
      this.metrics.onFollowBookmark();
  },

  /**
   * Update the loading progress of the document in response to a progress
   * message being received from the plugin.
   *
   * @param {number} progress the progress as a percentage.
   * @private
   */
  updateProgress_: function(progress) {
    if (this.toolbar_)
      this.toolbar_.loadProgress = progress;

    if (progress == -1) {
      // Document load failed.
      this.errorScreen_.show();
      this.sizer_.style.display = 'none';
      if (this.passwordScreen_.active) {
        this.passwordScreen_.deny();
        this.passwordScreen_.close();
      }
      this.loadState_ = LoadState.FAILED;
      this.isPrintPreviewLoadingFinished_ = true;
      this.sendDocumentLoadedMessage_();
    } else if (progress == 100) {
      // Document load complete.
      if (this.lastViewportPosition_)
        this.viewport_.position = this.lastViewportPosition_;
      this.paramsParser_.getViewportFromUrlParams(
          this.originalUrl_, this.handleURLParams_.bind(this));
      this.loadState_ = LoadState.SUCCESS;
      this.sendDocumentLoadedMessage_();
      while (this.delayedScriptingMessages_.length > 0)
        this.handleScriptingMessage(this.delayedScriptingMessages_.shift());

      this.toolbarManager_.hideToolbarsAfterTimeout();
    }
  },

  /**
   * Load a dictionary of translated strings into the UI. Used as a callback for
   * chrome.resourcesPrivate.
   *
   * @param {Object} strings Dictionary of translated strings
   * @private
   */
  handleStrings_: function(strings) {
    document.documentElement.dir = strings.textdirection;
    document.documentElement.lang = strings.language;

    $('toolbar').strings = strings;
    $('zoom-toolbar').strings = strings;
    $('password-screen').strings = strings;
    $('error-screen').strings = strings;
  },

  /**
   * An event handler for handling password-submitted events. These are fired
   * when an event is entered into the password screen.
   *
   * @param {Object} event a password-submitted event.
   * @private
   */
  onPasswordSubmitted_: function(event) {
    this.postMessage_(
        {type: 'getPasswordComplete', password: event.detail.password});
  },

  /**
   * Post a message to the PPAPI plugin. Some messages will cause an async reply
   * to be received through handlePluginMessage_().
   *
   * @param {Object} message Message to post.
   * @private
   */
  postMessage_: function(message) {
    this.plugin_.postMessage(message);
  },

  /**
   * An event handler for handling message events received from the plugin.
   *
   * @param {MessageObject} message a message event.
   * @private
   */
  handlePluginMessage_: function(message) {
    switch (message.data.type.toString()) {
      case 'beep':
        // Beeps are annoying, so just track count for now.
        this.beepCount_ += 1;
        break;
      case 'documentDimensions':
        this.documentDimensions_ = message.data;
        this.isUserInitiatedEvent_ = false;
        this.viewport_.setDocumentDimensions(this.documentDimensions_);
        this.isUserInitiatedEvent_ = true;
        // If we received the document dimensions, the password was good so we
        // can dismiss the password screen.
        if (this.passwordScreen_.active)
          this.passwordScreen_.close();

        if (this.pageIndicator_)
          this.pageIndicator_.initialFadeIn();

        if (this.toolbar_) {
          this.toolbar_.docLength =
              this.documentDimensions_.pageDimensions.length;
        }
        break;
      case 'email':
        var href = 'mailto:' + message.data.to + '?cc=' + message.data.cc +
            '&bcc=' + message.data.bcc + '&subject=' + message.data.subject +
            '&body=' + message.data.body;
        window.location.href = href;
        break;
      case 'getPassword':
        // If the password screen isn't up, put it up. Otherwise we're
        // responding to an incorrect password so deny it.
        if (!this.passwordScreen_.active)
          this.passwordScreen_.show();
        else
          this.passwordScreen_.deny();
        break;
      case 'getSelectedTextReply':
        this.sendScriptingMessage_(message.data);
        break;
      case 'goToPage':
        this.viewport_.goToPage(message.data.page);
        break;
      case 'loadProgress':
        this.updateProgress_(message.data.progress);
        break;
      case 'navigate':
        // If in print preview, always open a new tab.
        if (this.isPrintPreview_) {
          this.navigator_.navigate(
              message.data.url,
              Navigator.WindowOpenDisposition.NEW_BACKGROUND_TAB);
        } else {
          this.navigator_.navigate(message.data.url, message.data.disposition);
        }
        break;
      case 'printPreviewLoaded':
        this.isPrintPreviewLoadingFinished_ = true;
        this.sendDocumentLoadedMessage_();
        break;
      case 'setScrollPosition':
        this.viewport_.scrollTo(/** @type {!PartialPoint} */ (message.data));
        break;
      case 'scrollBy':
        this.viewport_.scrollBy(/** @type {!Point} */ (message.data));
        break;
      case 'cancelStreamUrl':
        chrome.mimeHandlerPrivate.abortStream();
        break;
      case 'metadata':
        if (message.data.title) {
          document.title = message.data.title;
        } else {
          document.title = getFilenameFromURL(this.originalUrl_);
        }
        this.bookmarks_ = message.data.bookmarks;
        if (this.toolbar_) {
          this.toolbar_.docTitle = document.title;
          this.toolbar_.bookmarks = this.bookmarks;
        }
        break;
      case 'setIsSelecting':
        this.viewportScroller_.setEnableScrolling(message.data.isSelecting);
        break;
      case 'getNamedDestinationReply':
        this.paramsParser_.onNamedDestinationReceived(message.data.pageNumber);
        break;
      case 'formFocusChange':
        this.isFormFieldFocused_ = message.data.focused;
        break;
      case 'transformPagePointReply':
        this.coordsTransformer_.onReplyReceived(message);
        break;
      case 'saveData':
        this.saveData_(message.data);
        break;
      case 'consumeSaveToken':
        if (!this.pendingTokens_.delete(message.data.token))
          throw new Error('Internal error: save token not found.');
        break;
    }
  },

  /**
   * Saves a pdf file buffer received from the plugin.
   *
   * @param {SaveDataMessageData} messageData data of the message event.
   * @private
   */
  saveData_: function(messageData) {
    // Verify a token that was created by this instance is included to avoid
    // being spammed.
    if (!this.pendingTokens_.delete(messageData.token))
      throw new Error('Internal error: save token not found, abort save.');

    // Verify the file size and the first bytes to make sure it's a PDF. Cap at
    // 100 MB. This cap should be kept in sync with and is also enforced in
    // pdf/out_of_process_instance.cc.
    const MIN_FILE_SIZE = '%PDF1.0'.length;
    const MAX_FILE_SIZE = 100 * 1000 * 1000;

    const bufView = new Uint8Array(messageData.dataToSave);
    if (bufView.length > MAX_FILE_SIZE)
      throw new Error(`File too large to be saved: ${bufView.length} bytes.`);
    if (bufView.length < MIN_FILE_SIZE ||
        String.fromCharCode(bufView[0], bufView[1], bufView[2], bufView[3]) !=
            '%PDF') {
      throw new Error('Not a PDF file.');
    }

    // Make sure file extension is .pdf, avoids dangerous extensions.
    let fileName = messageData.fileName;
    if (!fileName.toLowerCase().endsWith('.pdf'))
      fileName = fileName + '.pdf';

    const a = document.createElement('a');
    a.download = fileName;
    const blob = new Blob([messageData.dataToSave], {type: 'application/pdf'});
    a.href = URL.createObjectURL(blob);
    a.click();
    URL.revokeObjectURL(a.href);
  },

  /**
   * A callback that's called before the zoom changes. Notify the plugin to stop
   * reacting to scroll events while zoom is taking place to avoid flickering.
   *
   * @private
   */
  beforeZoom_: function() {
    this.postMessage_({type: 'stopScrolling'});

    if (this.viewport_.pinchPhase == Viewport.PinchPhase.PINCH_START) {
      var position = this.viewport_.position;
      var zoom = this.viewport_.zoom;
      var pinchPhase = this.viewport_.pinchPhase;
      this.postMessage_({
        type: 'viewport',
        userInitiated: true,
        zoom: zoom,
        xOffset: position.x,
        yOffset: position.y,
        pinchPhase: pinchPhase
      });
    }
  },

  /**
   * A callback that's called after the zoom changes. Notify the plugin of the
   * zoom change and to continue reacting to scroll events.
   *
   * @private
   */
  afterZoom_: function() {
    var position = this.viewport_.position;
    var zoom = this.viewport_.zoom;
    var pinchVector = this.viewport_.pinchPanVector || {x: 0, y: 0};
    var pinchCenter = this.viewport_.pinchCenter || {x: 0, y: 0};
    var pinchPhase = this.viewport_.pinchPhase;

    this.postMessage_({
      type: 'viewport',
      userInitiated: this.isUserInitiatedEvent_,
      zoom: zoom,
      xOffset: position.x,
      yOffset: position.y,
      pinchPhase: pinchPhase,
      pinchX: pinchCenter.x,
      pinchY: pinchCenter.y,
      pinchVectorX: pinchVector.x,
      pinchVectorY: pinchVector.y
    });
    this.zoomManager_.onPdfZoomChange();
  },

  /**
   * A callback that sets |isUserInitiatedEvent_| to |userInitiated|.
   *
   * @param {boolean} userInitiated The value to set |isUserInitiatedEvent_| to.
   * @private
   */
  setUserInitiated_: function(userInitiated) {
    if (this.isUserInitiatedEvent_ == userInitiated) {
      throw new Error('Trying to set user initiated to current value.');
    }
    this.isUserInitiatedEvent_ = userInitiated;
  },

  /**
   * A callback that's called when an update to a pinch zoom is detected.
   *
   * @param {!Object} e the pinch event.
   * @private
   */
  onPinchUpdate_: function(e) {
    // Throttle number of pinch events to one per frame.
    if (!this.sentPinchEvent_) {
      this.sentPinchEvent_ = true;
      window.requestAnimationFrame(() => {
        this.sentPinchEvent_ = false;
        this.viewport_.pinchZoom(e);
      });
    }
  },

  /**
   * A callback that's called when the end of a pinch zoom is detected.
   *
   * @param {!Object} e the pinch event.
   * @private
   */
  onPinchEnd_: function(e) {
    // Using rAF for pinch end prevents pinch updates scheduled by rAF getting
    // sent after the pinch end.
    window.requestAnimationFrame(() => {
      this.viewport_.pinchZoomEnd(e);
    });
  },

  /**
   * A callback that's called when the start of a pinch zoom is detected.
   *
   * @param {!Object} e the pinch event.
   * @private
   */
  onPinchStart_: function(e) {
    // We also use rAF for pinch start, so that if there is a pinch end event
    // scheduled by rAF, this pinch start will be sent after.
    window.requestAnimationFrame(() => {
      this.viewport_.pinchZoomStart(e);
    });
  },

  /**
   * A callback that's called after the viewport changes.
   *
   * @private
   */
  viewportChanged_: function() {
    if (!this.documentDimensions_)
      return;

    // Offset the toolbar position so that it doesn't move if scrollbars appear.
    var hasScrollbars = this.viewport_.documentHasScrollbars();
    var scrollbarWidth = this.viewport_.scrollbarWidth;
    var verticalScrollbarWidth = hasScrollbars.vertical ? scrollbarWidth : 0;
    var horizontalScrollbarWidth =
        hasScrollbars.horizontal ? scrollbarWidth : 0;

    // Shift the zoom toolbar to the left by half a scrollbar width. This
    // gives a compromise: if there is no scrollbar visible then the toolbar
    // will be half a scrollbar width further left than the spec but if there
    // is a scrollbar visible it will be half a scrollbar width further right
    // than the spec. In RTL layout, the zoom toolbar is on the left side, but
    // the scrollbar is still on the right, so this is not necessary.
    if (!isRTL()) {
      this.zoomToolbar_.style.right =
          -verticalScrollbarWidth + (scrollbarWidth / 2) + 'px';
    }
    // Having a horizontal scrollbar is much rarer so we don't offset the
    // toolbar from the bottom any more than what the spec says. This means
    // that when there is a scrollbar visible, it will be a full scrollbar
    // width closer to the bottom of the screen than usual, but this is ok.
    this.zoomToolbar_.style.bottom = -horizontalScrollbarWidth + 'px';

    // Update the page indicator.
    var visiblePage = this.viewport_.getMostVisiblePage();

    if (this.toolbar_)
      this.toolbar_.pageNo = visiblePage + 1;

    // TODO(raymes): Give pageIndicator_ the same API as toolbar_.
    if (this.pageIndicator_) {
      this.pageIndicator_.index = visiblePage;
      if (this.documentDimensions_.pageDimensions.length > 1 &&
          hasScrollbars.vertical) {
        this.pageIndicator_.style.visibility = 'visible';
      } else {
        this.pageIndicator_.style.visibility = 'hidden';
      }
    }

    var visiblePageDimensions = this.viewport_.getPageScreenRect(visiblePage);
    var size = this.viewport_.size;
    this.sendScriptingMessage_({
      type: 'viewport',
      pageX: visiblePageDimensions.x,
      pageY: visiblePageDimensions.y,
      pageWidth: visiblePageDimensions.width,
      viewportWidth: size.width,
      viewportHeight: size.height
    });
  },

  /**
   * Handle a scripting message from outside the extension (typically sent by
   * PDFScriptingAPI in a page containing the extension) to interact with the
   * plugin.
   *
   * @param {MessageObject} message the message to handle.
   */
  handleScriptingMessage: function(message) {
    if (this.parentWindow_ != message.source) {
      this.parentWindow_ = message.source;
      this.parentOrigin_ = message.origin;
      // Ensure that we notify the embedder if the document is loaded.
      if (this.loadState_ != LoadState.LOADING)
        this.sendDocumentLoadedMessage_();
    }

    if (this.handlePrintPreviewScriptingMessage_(message))
      return;

    // Delay scripting messages from users of the scripting API until the
    // document is loaded. This simplifies use of the APIs.
    if (this.loadState_ != LoadState.SUCCESS) {
      this.delayedScriptingMessages_.push(message);
      return;
    }

    switch (message.data.type.toString()) {
      case 'getSelectedText':
      case 'print':
      case 'selectAll':
        this.postMessage_(message.data);
        break;
    }
  },

  /**
   * Handle scripting messages specific to print preview.
   *
   * @param {MessageObject} message the message to handle.
   * @return {boolean} true if the message was handled, false otherwise.
   * @private
   */
  handlePrintPreviewScriptingMessage_: function(message) {
    if (!this.isPrintPreview_)
      return false;

    switch (message.data.type.toString()) {
      case 'loadPreviewPage':
        this.postMessage_(message.data);
        return true;
      case 'resetPrintPreviewMode':
        this.loadState_ = LoadState.LOADING;
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
        var printButton = $('print-button');
        if (printButton)
          printButton.parentNode.removeChild(printButton);
        var saveButton = $('save-button');
        if (saveButton)
          saveButton.parentNode.removeChild(saveButton);

        this.pageIndicator_.pageLabels = message.data.pageNumbers;

        this.postMessage_({
          type: 'resetPrintPreviewMode',
          url: message.data.url,
          grayscale: message.data.grayscale,
          // If the PDF isn't modifiable we send 0 as the page count so that no
          // blank placeholder pages get appended to the PDF.
          pageCount:
              (message.data.modifiable ? message.data.pageNumbers.length : 0)
        });
        return true;
      case 'sendKeyEvent':
        this.handleKeyEvent_(DeserializeKeyEvent(message.data.keyEvent));
        return true;
    }

    return false;
  },

  /**
   * Send a scripting message outside the extension (typically to
   * PDFScriptingAPI in a page containing the extension).
   *
   * @param {Object} message the message to send.
   * @private
   */
  sendScriptingMessage_: function(message) {
    if (this.parentWindow_ && this.parentOrigin_) {
      var targetOrigin;
      // Only send data back to the embedder if it is from the same origin,
      // unless we're sending it to ourselves (which could happen in the case
      // of tests). We also allow documentLoaded messages through as this won't
      // leak important information.
      if (this.parentOrigin_ == window.location.origin)
        targetOrigin = this.parentOrigin_;
      else if (message.type == 'documentLoaded')
        targetOrigin = '*';
      else
        targetOrigin = this.originalUrl_;
      this.parentWindow_.postMessage(message, targetOrigin);
    }
  },

  /**
   * @type {Viewport} the viewport of the PDF viewer.
   */
  get viewport() {
    return this.viewport_;
  },

  /**
   * Each bookmark is an Object containing a:
   * - title
   * - page (optional)
   * - array of children (themselves bookmarks)
   *
   * @type {Array} the top-level bookmarks of the PDF.
   */
  get bookmarks() {
    return this.bookmarks_;
  }
};
