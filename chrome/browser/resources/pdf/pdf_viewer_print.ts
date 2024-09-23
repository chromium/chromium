// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './elements/viewer_error_dialog.js';
import './elements/viewer_page_indicator.js';
import './elements/viewer_zoom_toolbar.js';

import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {isRTL} from 'chrome://resources/js/util.js';

import type {BrowserApi} from './browser_api.js';
import type {ExtendedKeyEvent} from './constants.js';
import {FittingType} from './constants.js';
import type {MessageData, PrintPreviewParams} from './controller.js';
import {PluginController} from './controller.js';
import type {ViewerPageIndicatorElement} from './elements/viewer_page_indicator.js';
import type {ViewerZoomToolbarElement} from './elements/viewer_zoom_toolbar.js';
import {convertDocumentDimensionsMessage, convertLoadProgressMessage} from './message_converter.js';
import {deserializeKeyEvent, LoadState, serializeKeyEvent} from './pdf_scripting_api.js';
import type {KeyEventData} from './pdf_viewer_base.js';
import {PdfViewerBaseElement} from './pdf_viewer_base.js';
import {getCss} from './pdf_viewer_print.css.js';
import {getHtml} from './pdf_viewer_print.html.js';
import {hasCtrlModifierOnly, shouldIgnoreKeyEvents} from './pdf_viewer_utils.js';
import {ToolbarManager} from './toolbar_manager.js';

let pluginLoaderPolicy: TrustedTypePolicy|null = null;

export interface PdfViewerPrintElement {
  $: {
    content: HTMLElement,
    pageIndicator: ViewerPageIndicatorElement,
    sizer: HTMLElement,
    zoomToolbar: ViewerZoomToolbarElement,
  };
}

export class PdfViewerPrintElement extends PdfViewerBaseElement {
  static get is() {
    return 'pdf-viewer-print';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  private isPrintPreviewLoadingFinished_: boolean = false;
  private inPrintPreviewMode_: boolean = false;
  private dark_: boolean = false;
  private pluginController_: PluginController|undefined = undefined;
  private toolbarManager_: ToolbarManager|null = null;

  override isNewUiEnabled() {
    return false;
  }

  getBackgroundColor() {
    return PRINT_PREVIEW_BACKGROUND_COLOR;
  }

  private getStreamUrl_(): TrustedScriptURL {
    if (pluginLoaderPolicy === null) {
      pluginLoaderPolicy =
          window.trustedTypes!.createPolicy('print-preview-plugin-loader', {
            createScriptURL: (_ignore: string) => {
              const url = new URL(this.browserApi!.getStreamInfo().streamUrl);

              // Checks based on data_request_filter.cc.
              assert(url.origin === 'chrome-untrusted://print');
              if (url.pathname.endsWith('test.pdf')) {
                return url.toString();
              }

              const paths = url.pathname.split('/');
              assert(paths.length === 4);
              assert(paths[3] === 'print.pdf');
              // Valid Print Preview UI ID
              assert(!Number.isNaN(parseInt(paths[1]!)));
              // Valid page index (can be negative for PDFs).
              assert(!Number.isNaN(parseInt(paths[2]!)));
              return url.toString();
            },
            createHTML: () => assertNotReached(),
            createScript: () => assertNotReached(),
          });
    }
    return pluginLoaderPolicy.createScriptURL('');
  }

  setPluginSrc(plugin: HTMLEmbedElement) {
    plugin.src = this.getStreamUrl_() as unknown as string;
  }

  init(browserApi: BrowserApi) {
    this.initInternal(
        browserApi, document.documentElement, this.$.sizer, this.$.content);

    this.pluginController_ = PluginController.getInstance();

    this.$.pageIndicator.setViewport(this.viewport);
    this.toolbarManager_ = new ToolbarManager(window, this.$.zoomToolbar);
  }

  handleKeyEvent(e: ExtendedKeyEvent) {
    if (shouldIgnoreKeyEvents() || e.defaultPrevented) {
      return;
    }

    this.toolbarManager_!.hideToolbarAfterTimeout();
    // Let the viewport handle directional key events.
    if (this.viewport.handleDirectionalKeyEvent(e, false)) {
      return;
    }

    switch (e.key) {
      case 'Tab':
        this.toolbarManager_!.showToolbarForKeyboardNavigation();
        return;
      case 'Escape':
        break;  // Ensure escape falls through to the print-preview handler.
      case 'a':
        if (hasCtrlModifierOnly(e)) {
          this.pluginController_!.selectAll();
          // Since we do selection ourselves.
          e.preventDefault();
        }
        return;
      case '\\':
        if (e.ctrlKey) {
          this.$.zoomToolbar.fitToggleFromHotKey();
        }
        return;
    }

    // Give print preview a chance to handle the key event.
    if (!e.fromScriptingAPI) {
      this.sendScriptingMessage(
          {type: 'sendKeyEvent', keyEvent: serializeKeyEvent(e)});
    } else {
      // Show toolbar as a fallback.
      if (!(e.shiftKey || e.ctrlKey || e.altKey)) {
        this.$.zoomToolbar.show();
      }
    }
  }

  private setBackgroundColorForPrintPreview_() {
    this.pluginController_!.setBackgroundColor(
        this.dark_ ? PRINT_PREVIEW_DARK_BACKGROUND_COLOR :
                     PRINT_PREVIEW_BACKGROUND_COLOR);
  }

  updateUiForViewportChange() {
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
    const zoomToolbar = this.$.zoomToolbar;
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
    const pageIndicator = this.$.pageIndicator;
    const lastIndex = pageIndicator.index;
    pageIndicator.index = visiblePage;
    if (this.documentDimensions!.pageDimensions.length > 1 &&
        hasScrollbars.vertical && lastIndex !== undefined) {
      pageIndicator.style.visibility = 'visible';
    } else {
      pageIndicator.style.visibility = 'hidden';
    }

    this.pluginController_!.viewportChanged();
  }

  override handleScriptingMessage(message: MessageEvent) {
    if (super.handleScriptingMessage(message)) {
      return true;
    }

    if (this.handlePrintPreviewScriptingMessage_(message)) {
      return true;
    }

    if (this.delayScriptingMessage(message)) {
      return true;
    }

    switch (message.data.type.toString()) {
      case 'getSelectedText':
        this.pluginController_!.getSelectedText().then(
            this.sendScriptingMessage.bind(this));
        break;
      case 'selectAll':
        this.pluginController_!.selectAll();
        break;
      default:
        return false;
    }
    return true;
  }

  /**
   * Handle scripting messages specific to print preview.
   * @param message the message to handle.
   * @return true if the message was handled, false otherwise.
   */
  private handlePrintPreviewScriptingMessage_(message: MessageEvent): boolean {
    const messageData = message.data;
    switch (messageData.type.toString()) {
      case 'loadPreviewPage':
        const loadData =
            messageData as MessageData & {url: string, index: number};
        this.pluginController_!.loadPreviewPage(loadData.url, loadData.index);
        return true;
      case 'resetPrintPreviewMode':
        const printPreviewData =
            messageData as (MessageData & PrintPreviewParams);
        this.setLoadState(LoadState.LOADING);
        if (!this.inPrintPreviewMode_) {
          this.inPrintPreviewMode_ = true;
          this.isUserInitiatedEvent = false;
          this.forceFit(FittingType.FIT_TO_PAGE);
          this.viewport.setFittingType(FittingType.FIT_TO_PAGE);
          this.isUserInitiatedEvent = true;
        }

        // Stash the scroll location so that it can be restored when the new
        // document is loaded.
        this.lastViewportPosition = this.viewport.position;
        this.$.pageIndicator.pageLabels = printPreviewData.pageNumbers;

        this.pluginController_!.resetPrintPreviewMode(printPreviewData);
        return true;
      case 'sendKeyEvent':
        const keyEvent =
            deserializeKeyEvent((message.data as KeyEventData).keyEvent);
        const extendedKeyEvent = keyEvent as ExtendedKeyEvent;
        extendedKeyEvent.fromScriptingAPI = true;
        this.handleKeyEvent(extendedKeyEvent);
        return true;
      case 'hideToolbar':
        this.toolbarManager_!.resetKeyboardNavigationAndHideToolbar();
        return true;
      case 'darkModeChanged':
        this.dark_ =
            (message.data as (MessageData & {darkMode: boolean})).darkMode;
        this.setBackgroundColorForPrintPreview_();
        return true;
      case 'scrollPosition':
        const position = this.viewport.position;
        const positionData =
            message.data as (MessageData & {x: number, y: number});
        position.y += positionData.y;
        position.x += positionData.x;
        this.viewport.setPosition(position);
        return true;
    }

    return false;
  }

  override setLoadState(loadState: LoadState) {
    super.setLoadState(loadState);
    if (loadState === LoadState.FAILED) {
      this.isPrintPreviewLoadingFinished_ = true;
    }
  }

  override handlePluginMessage(e: CustomEvent) {
    const data = e.detail;
    switch (data.type.toString()) {
      case 'documentDimensions':
        this.setDocumentDimensions(convertDocumentDimensionsMessage(data));
        return;
      case 'documentFocusChanged':
        // TODO(crbug.com/40125884): Draw a focus rect around plugin.
        return;
      case 'loadProgress':
        this.updateProgress(convertLoadProgressMessage(data).progress);
        return;
      case 'printPreviewLoaded':
        this.handlePrintPreviewLoaded_();
        return;
      case 'sendKeyEvent':
        const keyEvent = deserializeKeyEvent((data as KeyEventData).keyEvent) as
            ExtendedKeyEvent;
        keyEvent.fromPlugin = true;
        this.handleKeyEvent(keyEvent);
        return;
      case 'touchSelectionOccurred':
        this.sendScriptingMessage({
          type: 'touchSelectionOccurred',
        });
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
   */
  private handlePrintPreviewLoaded_() {
    this.isPrintPreviewLoadingFinished_ = true;
    this.sendDocumentLoadedMessage();
  }

  override readyToSendLoadMessage() {
    return this.isPrintPreviewLoadingFinished_;
  }

  forceFit(view: FittingType) {
    this.$.zoomToolbar.forceFit(view);
  }

  protected afterZoom(_viewportZoom: number) {}

  override handleStrings(strings: {[key: string]: string}) {
    super.handleStrings(strings);
    if (!strings) {
      return;
    }
    this.setBackgroundColorForPrintPreview_();
  }

  override updateProgress(progress: number) {
    super.updateProgress(progress);
    if (progress === 100) {
      this.toolbarManager_!.hideToolbarAfterTimeout();
    }
  }
}

/**
 * The background color used for print preview (--google-grey-300). Keep
 * in sync with `ChromePdfStreamDelegate::MapToOriginalUrl()`.
 */
const PRINT_PREVIEW_BACKGROUND_COLOR: number = 0xffdadce0;

/**
 * The background color used for print preview when dark mode is enabled
 * (--google-grey-700).
 */
const PRINT_PREVIEW_DARK_BACKGROUND_COLOR: number = 0xff5f6368;

declare global {
  interface HTMLElementTagNameMap {
    'pdf-viewer-print': PdfViewerPrintElement;
  }
}

customElements.define(PdfViewerPrintElement.is, PdfViewerPrintElement);
