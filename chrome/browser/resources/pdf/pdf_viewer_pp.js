// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './elements/viewer-error-screen.js';
import './elements/viewer-page-indicator.js';
import './elements/viewer-zoom-toolbar.js';
import './elements/shared-vars.js';
import './pdf_viewer_shared_style.js';

import {assertNotReached} from 'chrome://resources/js/assert.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {isRTL} from 'chrome://resources/js/util.m.js';
import {html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserApi} from './browser_api.js';
import {FittingType} from './constants.js';
import {MessageData, PluginController, PrintPreviewParams} from './controller.js';
import {ViewerErrorScreenElement} from './elements/viewer-error-screen.js';
import {DeserializeKeyEvent, LoadState, SerializeKeyEvent} from './pdf_scripting_api.js';
import {PDFViewerBaseElement} from './pdf_viewer_base.js';
import {DestinationMessageData, DocumentDimensionsMessageData, MessageObject, shouldIgnoreKeyEvents} from './pdf_viewer_utils.js';
import {ToolbarManager} from './toolbar_manager.js';

class PDFViewerPPElement extends PDFViewerBaseElement {
  static get is() {
    return 'pdf-viewer-pp';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  constructor() {
    super();

    /** @private {boolean} */
    this.isPrintPreviewLoadingFinished_ = false;

    /** @private {boolean} */
    this.inPrintPreviewMode_ = false;

    /** @private {boolean} */
    this.dark_ = false;

    /** @private {?ToolbarManager} */
    this.toolbarManager_ = null;
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

  /** @override */
  getBackgroundColor() {
    return PRINT_PREVIEW_BACKGROUND_COLOR;
  }

  /**
   * @return {!ViewerZoomToolbarElement}
   * @private
   */
  getZoomToolbar_() {
    return /** @type {!ViewerZoomToolbarElement} */ (this.$$('#zoom-toolbar'));
  }

  /** @param {!BrowserApi} browserApi */
  init(browserApi) {
    super.init(browserApi);

    /** @private {?PluginController} */
    this.pluginController_ = PluginController.getInstance();

    this.toolbarManager_ = new ToolbarManager(window, this.getZoomToolbar_());

    // Setup the keyboard event listener.
    document.addEventListener(
        'keydown',
        e => this.handleKeyEvent_(/** @type {!KeyboardEvent} */ (e)));
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

    this.toolbarManager_.hideToolbarAfterTimeout();
    // Let the viewport handle directional key events.
    if (this.viewport.handleDirectionalKeyEvent(e, false)) {
      return;
    }

    switch (e.key) {
      case 'Tab':
        this.toolbarManager_.showToolbarForKeyboardNavigation();
        return;
      case 'Escape':
        break;  // Ensure escape falls through to the print-preview handler.
      case 'a':
        if (e.ctrlKey || e.metaKey) {
          this.pluginController_.selectAll();
          // Since we do selection ourselves.
          e.preventDefault();
        }
        return;
      case '\\':
        if (e.ctrlKey) {
          this.getZoomToolbar_().fitToggleFromHotKey();
        }
        return;
    }

    // Give print preview a chance to handle the key event.
    if (!e.fromScriptingAPI) {
      this.sendScriptingMessage(
          {type: 'sendKeyEvent', keyEvent: SerializeKeyEvent(e)});
    } else {
      // Show toolbar as a fallback.
      if (!(e.shiftKey || e.ctrlKey || e.altKey)) {
        this.getZoomToolbar_().show();
      }
    }
  }

  /** @private */
  setBackgroundColorForPrintPreview_() {
    this.pluginController_.setBackgroundColor(
        this.dark_ ? PRINT_PREVIEW_DARK_BACKGROUND_COLOR :
                     PRINT_PREVIEW_BACKGROUND_COLOR);
  }

  /** @override */
  updateUIForViewportChange() {
    // Offset the toolbar position so that it doesn't move if scrollbars appear.
    const hasScrollbars = this.viewport.documentHasScrollbars();
    const scrollbarWidth = this.viewport.scrollbarWidth;
    const verticalScrollbarWidth = hasScrollbars.vertical ? scrollbarWidth : 0;
    const horizontalScrollbarWidth =
        hasScrollbars.horizontal ? scrollbarWidth : 0;

    // Shift the zoom toolbar to the left by half a scrollbar width. This
    // gives a compromise: if there is no scrollbar visible then the toolbar
    // will be half a scrollbar width further left than the spec but if there
    // is a scrollbar visible it will be half a scrollbar width further right
    // than the spec. In LTR layout, the zoom toolbar is on the left
    // left side, but the scrollbar is still on the right, so this is not
    // necessary.
    const zoomToolbar = this.getZoomToolbar_();
    if (isRTL()) {
      zoomToolbar.style.right =
          -verticalScrollbarWidth + (scrollbarWidth / 2) + 'px';
    }
    // Having a horizontal scrollbar is much rarer so we don't offset the
    // toolbar from the bottom any more than what the spec says. This means
    // that when there is a scrollbar visible, it will be a full scrollbar
    // width closer to the bottom of the screen than usual, but this is ok.
    zoomToolbar.style.bottom = -horizontalScrollbarWidth + 'px';

    // Update the page indicator.
    const visiblePage = this.viewport.getMostVisiblePage();
    const pageIndicator = this.$$('#page-indicator');
    const lastIndex = pageIndicator.index;
    pageIndicator.index = visiblePage;
    if (this.documentDimensions.pageDimensions.length > 1 &&
        hasScrollbars.vertical && lastIndex !== undefined) {
      pageIndicator.style.visibility = 'visible';
    } else {
      pageIndicator.style.visibility = 'hidden';
    }

    this.pluginController_.viewportChanged();
  }

  /** @override */
  handleScriptingMessage(message) {
    super.handleScriptingMessage(message);

    if (this.handlePrintPreviewScriptingMessage_(message)) {
      return;
    }

    if (this.delayScriptingMessage(message)) {
      return;
    }

    switch (message.data.type.toString()) {
      case 'getSelectedText':
        this.pluginController_.getSelectedText().then(
            this.sendScriptingMessage.bind(this));
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
        this.setLoadState(LoadState.LOADING);
        if (!this.inPrintPreviewMode_) {
          this.inPrintPreviewMode_ = true;
          this.isUserInitiatedEvent = false;
          this.forceFit(FittingType.FIT_TO_PAGE);
          this.updateViewportFit(FittingType.FIT_TO_PAGE);
          this.isUserInitiatedEvent = true;
        }

        // Stash the scroll location so that it can be restored when the new
        // document is loaded.
        this.lastViewportPosition = this.viewport.position;
        this.$$('#page-indicator').pageLabels = messageData.pageNumbers;

        this.pluginController_.resetPrintPreviewMode(messageData);
        return true;
      case 'sendKeyEvent':
        this.handleKeyEvent_(/** @type {!KeyboardEvent} */ (DeserializeKeyEvent(
            /** @type {{ keyEvent: Object }} */ (message.data).keyEvent)));
        return true;
      case 'hideToolbar':
        this.toolbarManager_.resetKeyboardNavigationAndHideToolbar();
        return true;
      case 'darkModeChanged':
        this.dark_ = /** @type {{darkMode: boolean}} */ (message.data).darkMode;
        this.setBackgroundColorForPrintPreview_();
        return true;
      case 'scrollPosition':
        const position = this.viewport.position;
        messageData = /** @type {{ x: number, y: number }} */ (message.data);
        position.y += messageData.y;
        position.x += messageData.x;
        this.viewport.position = position;
        return true;
    }

    return false;
  }

  /** @override */
  setLoadState(loadState) {
    super.setLoadState(loadState);
    if (loadState === LoadState.FAILED) {
      this.isPrintPreviewLoadingFinished_ = true;
    }
  }

  /** @override */
  handlePluginMessage(e) {
    const data = e.detail;
    switch (data.type.toString()) {
      case 'documentDimensions':
        this.setDocumentDimensions(
            /** @type {!DocumentDimensionsMessageData} */ (data));
        return;
      case 'loadProgress':
        this.updateProgress(
            /** @type {{ progress: number }} */ (data).progress);
        return;
      case 'navigateToDestination':
        const destinationData = /** @type {!DestinationMessageData} */ (data);
        this.viewport.handleNavigateToDestination(
            destinationData.page, destinationData.x, destinationData.y,
            destinationData.zoom);
        return;
      case 'printPreviewLoaded':
        this.handlePrintPreviewLoaded_();
        return;
      case 'setIsSelecting':
        this.viewportScroller.setEnableScrolling(
            /** @type {{ isSelecting: boolean }} */ (data).isSelecting);
        return;
      case 'touchSelectionOccurred':
        this.sendScriptingMessage({
          type: 'touchSelectionOccurred',
        });
        return;
      case 'documentFocusChanged':
        // TODO(crbug.com/1069370): Draw a focus rect around plugin.
        return;
      case 'beep':
      case 'formFocusChange':
      case 'getPassword':
      case 'metadata':
      case 'navigate':
      case 'setIsEditing':
        // These messages are not relevant in Print Preview.
        return;
    }
    assertNotReached('Unknown message type received: ' + data.type);
  }

  /**
   * Handles a notification that print preview has loaded from the
   * current controller.
   * @private
   */
  handlePrintPreviewLoaded_() {
    this.isPrintPreviewLoadingFinished_ = true;
    this.sendDocumentLoadedMessage();
  }

  /** @override */
  readyToSendLoadMessage() {
    return this.isPrintPreviewLoadingFinished_;
  }

  /** @override */
  forceFit(view) {
    this.getZoomToolbar_().forceFit(view);
  }

  /** @override */
  handleStrings(strings) {
    super.handleStrings(strings);
    if (!strings) {
      return;
    }
    this.setBackgroundColorForPrintPreview_();
  }

  /** @override */
  updateProgress(progress) {
    super.updateProgress(progress);
    if (progress === 100) {
      this.toolbarManager_.hideToolbarAfterTimeout();
    }
  }
}

/**
 * The background color used for print preview (--google-grey-refresh-300).
 * @type {number}
 */
const PRINT_PREVIEW_BACKGROUND_COLOR = 0xffdadce0;

/**
 * The background color used for print preview when dark mode is enabled
 * (--google-grey-refresh-700).
 * @type {number}
 */
const PRINT_PREVIEW_DARK_BACKGROUND_COLOR = 0xff5f6368;

customElements.define(PDFViewerPPElement.is, PDFViewerPPElement);
