// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Wraps the Ink component with an API that can be called
 * across an IFrame boundary.
 */
class InkAPI {
  /** @param {!drawings.Canvas} canvas */
  constructor(canvas) {
    this.canvas_ = canvas;
    this.camera_ = null;
  }

  /** @param {function(!drawings.UndoState)} listener */
  addUndoStateListener(listener) {
    this.canvas_.addUndoRedoListener(listener);
  }

  /**
   * @param {!ArrayBuffer} buffer
   */
  async setPDF(buffer) {
    // We change the type from ArrayBuffer to Uint8Array due to the consequences
    // of the buffer being passed across the iframe boundary. This realm has a
    // different ArrayBuffer constructor than `buffer`.
    return this.canvas_.setPDF(new Uint8Array(buffer));
  }

  /**
   * @return {!Uint8Array}
   */
  getPDF() {
    return this.canvas_.getPDF();
  }

  /**
   * @return {!Uint8Array}
   */
  getPDFDestructive() {
    return this.canvas_.getPDFDestructive();
  }

  async setCamera(camera) {
    this.camera_ = camera;
    this.canvas_.setCamera(camera);
    // Wait for the next task to avoid a race where Ink drops the camera value
    // when the canvas is rotated in low-latency mode.
    setTimeout(() => this.canvas_.setCamera(this.camera_), 0);
  }

  /** @param {AnnotationTool} tool */
  setAnnotationTool(tool) {
    this.canvas_.setTool(tool);
  }

  flush() {
    return this.canvas_.flush();
  }

  /** @param {string} hexColor */
  setOutOfBoundsColor(hexColor) {
    this.canvas_.setOutOfBoundsColor(hexColor);
  }

  /** @param {string} url */
  setBorderImage(url) {
    this.canvas_.setBorderImage(url);
  }

  /** @param {number} spacing in points */
  setPageSpacing(spacing) {
    this.canvas_.setVerticalPageLayout(spacing);
  }

  dispatchPointerEvent(evt) {
    this.canvas_.dispatchInput(evt);
  }

  undo() {
    this.canvas_.undo();
  }

  redo() {
    this.canvas_.redo();
  }
}

/** @return {Promise<InkAPI>} */
window.initInk = async function() {
  const canvas = await drawings.Canvas.execute(document.body);
  return new InkAPI(canvas);
};
