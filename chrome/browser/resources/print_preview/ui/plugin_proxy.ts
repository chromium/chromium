// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PdfPlugin} from 'chrome://print/pdf/pdf_scripting_api.js';
import {pdfCreateOutOfProcessPlugin} from 'chrome://print/pdf/pdf_scripting_api.js';
import {assert} from 'chrome://resources/js/assert.js';

export type ViewportChangedCallback =
    (pageX: number, pageY: number, pageWidth: number, viewportWidth: number,
     viewportHeight: number) => void;

/**
 * An interface to the PDF plugin.
 */
export interface PluginProxy {
  /** @return Whether the plugin is ready. */
  pluginReady(): boolean;

  /**
   * Creates the PDF plugin.
   * @param previewUid The unique ID of the preview UI.
   * @param index The preview index to load.
   * @return The created plugin.
   */
  createPlugin(previewUid: number, index: number): PdfPlugin;

  /**
   * @param previewUid Unique identifier of preview.
   * @param index Page index for plugin.
   * @param color Whether the preview should be color.
   * @param pages Page indices to preview.
   * @param modifiable Whether the document is modifiable.
   */
  resetPrintPreviewMode(
      previewUid: number, index: number, color: boolean, pages: number[],
      modifiable: boolean): void;

  /**
   * @param scrollX The amount to horizontally scroll in pixels.
   * @param scrollY The amount to vertically scroll in pixels.
   */
  scrollPosition(scrollX: number, scrollY: number): void;

  /** @param e Keyboard event to forward to the plugin. */
  sendKeyEvent(e: KeyboardEvent): void;

  hideToolbar(): void;

  /**
   * @param eventsEnabled Whether pointer events should be captured by the
   *     plugin.
   */
  setPointerEvents(eventsEnabled: boolean): void;

  /**
   * @param previewUid The unique ID of the preview UI.
   * @param pageIndex The page index to load.
   * @param index The preview index.
   */
  loadPreviewPage(previewUid: number, pageIndex: number, index: number): void;

  setKeyEventCallback(keyEventCallback: (e: KeyboardEvent) => void): void;

  setLoadCompleteCallback(loadCompleteCallback: (success: boolean) => void):
      void;

  setViewportChangedCallback(
      viewportChangedCallback:
          (pageX: number, pageY: number, pageWidth: number,
           viewportWidth: number, viewportHeight: number) => void): void;

  /** @param darkMode Whether the page is in dark mode. */
  darkModeChanged(darkMode: boolean): void;
}

export class PluginProxyImpl implements PluginProxy {
  private plugin_: PdfPlugin|null = null;

  pluginReady() {
    return !!this.plugin_;
  }

  createPlugin(previewUid: number, index: number) {
    assert(!this.plugin_);
    const srcUrl = this.getPreviewUrl_(previewUid, index);
    this.plugin_ = pdfCreateOutOfProcessPlugin(
        srcUrl, 'chrome://print/pdf/index_print.html');
    this.plugin_!.classList.add('preview-area-plugin');
    // NOTE: The plugin's 'id' field must be set to 'pdf-viewer' since
    // chrome/renderer/printing/print_render_frame_helper.cc actually
    // references it.
    this.plugin_!.setAttribute('id', 'pdf-viewer');
    return this.plugin_!;
  }

  /**
   * Get the URL for the plugin.
   * @param previewUid Unique identifier of preview.
   * @param index Page index for plugin.
   */
  private getPreviewUrl_(previewUid: number, index: number): string {
    return `chrome-untrusted://print/${previewUid}/${index}/print.pdf`;
  }

  resetPrintPreviewMode(
      previewUid: number, index: number, color: boolean, pages: number[],
      modifiable: boolean) {
    this.plugin_!.resetPrintPreviewMode(
        this.getPreviewUrl_(previewUid, index), color, pages, modifiable);
  }

  scrollPosition(scrollX: number, scrollY: number) {
    this.plugin_!.scrollPosition(scrollX, scrollY);
  }

  sendKeyEvent(e: KeyboardEvent) {
    this.plugin_!.sendKeyEvent(e);
  }

  hideToolbar() {
    this.plugin_!.hideToolbar();
  }

  setPointerEvents(eventsEnabled: boolean) {
    this.plugin_!.style.pointerEvents = eventsEnabled ? 'auto' : 'none';
  }

  loadPreviewPage(previewUid: number, pageIndex: number, index: number) {
    this.plugin_!.loadPreviewPage(
        this.getPreviewUrl_(previewUid, pageIndex), index);
  }

  setKeyEventCallback(keyEventCallback: (e: KeyboardEvent) => void) {
    this.plugin_!.setKeyEventCallback(keyEventCallback);
  }

  setLoadCompleteCallback(loadCompleteCallback: (success: boolean) => void) {
    this.plugin_!.setLoadCompleteCallback(loadCompleteCallback);
  }

  setViewportChangedCallback(viewportChangedCallback: ViewportChangedCallback) {
    this.plugin_!.setViewportChangedCallback(viewportChangedCallback);
  }

  darkModeChanged(darkMode: boolean) {
    this.plugin_!.darkModeChanged(darkMode);
  }

  static setInstance(obj: PluginProxy): void {
    instance = obj;
  }

  static getInstance(): PluginProxy {
    return instance || (instance = new PluginProxyImpl());
  }
}

let instance: PluginProxy|null = null;
