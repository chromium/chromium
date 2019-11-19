// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';
import {PDFCreateOutOfProcessPlugin} from '../pdf/pdf_scripting_api.js';

/**
 * @typedef {{accessibility: Function,
 *            documentLoadComplete: Function,
 *            getHeight: Function,
 *            getHorizontalScrollbarThickness: Function,
 *            getPageLocationNormalized: Function,
 *            getVerticalScrollbarThickness: Function,
 *            getWidth: Function,
 *            getZoomLevel: Function,
 *            goToPage: Function,
 *            grayscale: Function,
 *            loadPreviewPage: Function,
 *            onload: Function,
 *            onPluginSizeChanged: Function,
 *            onScroll: Function,
 *            pageXOffset: Function,
 *            pageYOffset: Function,
 *            reload: Function,
 *            resetPrintPreviewMode: Function,
 *            sendKeyEvent: Function,
 *            setPageNumbers: Function,
 *            setPageXOffset: Function,
 *            setPageYOffset: Function,
 *            setZoomLevel: Function,
 *            fitToHeight: Function,
 *            fitToWidth: Function,
 *            zoomIn: Function,
 *            zoomOut: Function}}
 */
export let PDFPlugin;

/**
 * An interface to the PDF plugin.
 */
export class PluginProxy {
  /**
   * Creates a new PluginProxy if the current instance is not set.
   * @return {!PluginProxy} The singleton instance.
   */
  static getInstance() {
    if (instance == null) {
      instance = new PluginProxy();
    }
    return assert(instance);
  }

  /**
   * @param {!PluginProxy} newInstance The PluginProxy
   *     instance to set for print preview construction.
   */
  static setInstance(newInstance) {
    instance = newInstance;
  }

  constructor() {
    /** @private {?PDFPlugin} */
    this.plugin_ = null;
  }

  /**
   * @param {!Element} oopCompatObj The out of process compatibility element.
   * @return {boolean} Whether the plugin exists and is compatible.
   */
  checkPluginCompatibility(oopCompatObj) {
    const isOOPCompatible = oopCompatObj.postMessage;
    oopCompatObj.parentElement.removeChild(oopCompatObj);

    return isOOPCompatible;
  }

  /** @return {boolean} Whether the plugin is ready. */
  pluginReady() {
    return !!this.plugin_;
  }

  /**
   * Creates the PDF plugin.
   * @param {number} previewUid The unique ID of the preview UI.
   * @param {number} index The preview index to load.
   * @return {!PDFPlugin} The created plugin.
   */
  createPlugin(previewUid, index) {
    assert(!this.plugin_);
    const srcUrl = this.getPreviewUrl_(previewUid, index);
    this.plugin_ = /** @type {PDFPlugin} */ (
        PDFCreateOutOfProcessPlugin(srcUrl, 'chrome://print/pdf'));
    this.plugin_.classList.add('preview-area-plugin');
    this.plugin_.setAttribute('aria-live', 'polite');
    this.plugin_.setAttribute('aria-atomic', 'true');
    // NOTE: The plugin's 'id' field must be set to 'pdf-viewer' since
    // chrome/renderer/printing/print_render_frame_helper.cc actually
    // references it.
    this.plugin_.setAttribute('id', 'pdf-viewer');
    return this.plugin_;
  }

  /**
   * Get the URL for the plugin.
   * @param {number} previewUid Unique identifier of preview.
   * @param {number} index Page index for plugin.
   * @return {string} The URL
   * @private
   */
  getPreviewUrl_(previewUid, index) {
    return `chrome://print/${previewUid}/${index}/print.pdf`;
  }

  /**
   * @param {number} previewUid Unique identifier of preview.
   * @param {number} index Page index for plugin.
   * @param {boolean} color Whether the preview should be color.
   * @param {!Array<number>} pages Page indices to preview.
   * @param {boolean} modifiable Whether the document is modifiable.
   */
  resetPrintPreviewMode(previewUid, index, color, pages, modifiable) {
    this.plugin_.resetPrintPreviewMode(
        this.getPreviewUrl_(previewUid, index), color, pages, modifiable);
  }

  /**
   * @param {number} scrollX The amount to horizontally scroll in pixels.
   * @param {number} scrollY The amount to vertically scroll in pixels.
   */
  scrollPosition(scrollX, scrollY) {
    this.plugin_.scrollPosition(scrollX, scrollY);
  }

  /** @param {!KeyboardEvent} e Keyboard event to forward to the plugin. */
  sendKeyEvent(e) {
    this.plugin_.sendKeyEvent(e);
  }

  hideToolbars() {
    this.plugin_.hideToolbars();
  }

  /**
   * @param {boolean} eventsEnabled Whether pointer events should be captured
   *     by the plugin.
   */
  setPointerEvents(eventsEnabled) {
    this.plugin_.style.pointerEvents = eventsEnabled ? 'auto' : 'none';
  }

  /**
   * @param {number} previewUid The unique ID of the preview UI.
   * @param {number} pageIndex The page index to load.
   * @param {number} index The preview index.
   */
  loadPreviewPage(previewUid, pageIndex, index) {
    this.plugin_.loadPreviewPage(
        this.getPreviewUrl_(previewUid, pageIndex), index);
  }

  /** @param {?Function} keyEventCallback */
  setKeyEventCallback(keyEventCallback) {
    this.plugin_.setKeyEventCallback(keyEventCallback);
  }

  /** @param {?Function} loadCallback */
  setLoadCallback(loadCallback) {
    this.plugin_.setLoadCallback(loadCallback);
  }

  /** @param {?Function} viewportChangedCallback */
  setViewportChangedCallback(viewportChangedCallback) {
    this.plugin_.setViewportChangedCallback(viewportChangedCallback);
  }

  /** @param {boolean} darkMode Whether the page is in dark mode. */
  darkModeChanged(darkMode) {
    this.plugin_.darkModeChanged(darkMode);
  }
}

/** @type {?PluginProxy} */
let instance = null;
