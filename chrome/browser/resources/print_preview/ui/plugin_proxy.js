// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';
import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';

import {PDFCreateOutOfProcessPlugin} from '../pdf/pdf_scripting_api.js';

/**
 * @typedef {{darkModeChanged: Function,
 *            hideToolbar: Function,
 *            loadPreviewPage: Function,
 *            resetPrintPreviewMode: Function,
 *            scrollPosition: Function,
 *            sendKeyEvent: Function,
 *            setKeyEventCallback: Function,
 *            setLoadCompleteCallback: Function,
 *            setViewportChangedCallback: Function}}
 */
export let PDFPlugin;

/**
 * An interface to the PDF plugin.
 * @interface
 */
export class PluginProxy {
  /**
   * @param {!Element} oopCompatObj The out of process compatibility element.
   * @return {boolean} Whether the plugin exists and is compatible.
   */
  checkPluginCompatibility(oopCompatObj) {}

  /** @return {boolean} Whether the plugin is ready. */
  pluginReady() {}

  /**
   * Creates the PDF plugin.
   * @param {number} previewUid The unique ID of the preview UI.
   * @param {number} index The preview index to load.
   * @return {!PDFPlugin} The created plugin.
   */
  createPlugin(previewUid, index) {}

  /**
   * @param {number} previewUid Unique identifier of preview.
   * @param {number} index Page index for plugin.
   * @param {boolean} color Whether the preview should be color.
   * @param {!Array<number>} pages Page indices to preview.
   * @param {boolean} modifiable Whether the document is modifiable.
   */
  resetPrintPreviewMode(previewUid, index, color, pages, modifiable) {}

  /**
   * @param {number} scrollX The amount to horizontally scroll in pixels.
   * @param {number} scrollY The amount to vertically scroll in pixels.
   */
  scrollPosition(scrollX, scrollY) {}

  /** @param {!KeyboardEvent} e Keyboard event to forward to the plugin. */
  sendKeyEvent(e) {}

  hideToolbar() {}

  /**
   * @param {boolean} eventsEnabled Whether pointer events should be captured
   *     by the plugin.
   */
  setPointerEvents(eventsEnabled) {}

  /**
   * @param {number} previewUid The unique ID of the preview UI.
   * @param {number} pageIndex The page index to load.
   * @param {number} index The preview index.
   */
  loadPreviewPage(previewUid, pageIndex, index) {}

  /** @param {?Function} keyEventCallback */
  setKeyEventCallback(keyEventCallback) {}

  /** @param {?Function} loadCompleteCallback */
  setLoadCompleteCallback(loadCompleteCallback) {}

  /** @param {?Function} viewportChangedCallback */
  setViewportChangedCallback(viewportChangedCallback) {}

  /** @param {boolean} darkMode Whether the page is in dark mode. */
  darkModeChanged(darkMode) {}
}

/** @implements {PluginProxy} */
export class PluginProxyImpl {
  constructor() {
    /** @private {?PDFPlugin} */
    this.plugin_ = null;
  }

  /** @override */
  checkPluginCompatibility(oopCompatObj) {
    const isOOPCompatible = oopCompatObj.postMessage;
    oopCompatObj.parentElement.removeChild(oopCompatObj);

    return isOOPCompatible;
  }

  /** @override */
  pluginReady() {
    return !!this.plugin_;
  }

  /** @override */
  createPlugin(previewUid, index) {
    assert(!this.plugin_);
    const srcUrl = this.getPreviewUrl_(previewUid, index);
    this.plugin_ = /** @type {PDFPlugin} */ (
        PDFCreateOutOfProcessPlugin(srcUrl, 'chrome://print/pdf'));
    this.plugin_.classList.add('preview-area-plugin');
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

  /** @override */
  resetPrintPreviewMode(previewUid, index, color, pages, modifiable) {
    this.plugin_.resetPrintPreviewMode(
        this.getPreviewUrl_(previewUid, index), color, pages, modifiable);
  }

  /** @override */
  scrollPosition(scrollX, scrollY) {
    this.plugin_.scrollPosition(scrollX, scrollY);
  }

  /** @override */
  sendKeyEvent(e) {
    this.plugin_.sendKeyEvent(e);
  }

  /** @override */
  hideToolbar() {
    this.plugin_.hideToolbar();
  }

  /** @override */
  setPointerEvents(eventsEnabled) {
    this.plugin_.style.pointerEvents = eventsEnabled ? 'auto' : 'none';
  }

  /** @override */
  loadPreviewPage(previewUid, pageIndex, index) {
    this.plugin_.loadPreviewPage(
        this.getPreviewUrl_(previewUid, pageIndex), index);
  }

  /** @override */
  setKeyEventCallback(keyEventCallback) {
    this.plugin_.setKeyEventCallback(keyEventCallback);
  }

  /** @override */
  setLoadCompleteCallback(loadCompleteCallback) {
    this.plugin_.setLoadCompleteCallback(loadCompleteCallback);
  }

  /** @override */
  setViewportChangedCallback(viewportChangedCallback) {
    this.plugin_.setViewportChangedCallback(viewportChangedCallback);
  }

  /** @override */
  darkModeChanged(darkMode) {
    this.plugin_.darkModeChanged(darkMode);
  }
}

addSingletonGetter(PluginProxyImpl);
