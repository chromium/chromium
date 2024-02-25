// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {AnnotationTool} from '../annotation_tool.js';

/**
 * Wraps the Ink component with an API that can be called
 * across an IFrame boundary.
 */
export class InkApi {
  private canvas_: drawings.Canvas;
  private camera_: drawings.Box|null = null;

  constructor(canvas: drawings.Canvas) {
    this.canvas_ = canvas;
    this.camera_ = null;
  }

  addUndoStateListener(listener: (state: drawings.UndoState) => void) {
    this.canvas_.addUndoRedoListener(listener);
  }

  async setPdf(buffer: ArrayBuffer) {
    // We change the type from ArrayBuffer to Uint8Array due to the consequences
    // of the buffer being passed across the iframe boundary. This realm has a
    // different ArrayBuffer constructor than `buffer`.
    return this.canvas_.setPDF(new Uint8Array(buffer));
  }

  async getPdf(): Promise<Uint8Array> {
    return this.canvas_.getPDF();
  }

  async getPdfDestructive(): Promise<Uint8Array> {
    return this.canvas_.getPDFDestructive();
  }

  async setCamera(camera: drawings.Box) {
    this.camera_ = camera;
    this.canvas_.setCamera(camera);
    // Wait for the next task to avoid a race where Ink drops the camera value
    // when the canvas is rotated in low-latency mode.
    setTimeout(() => this.canvas_.setCamera(this.camera_!), 0);
  }

  setAnnotationTool(tool: AnnotationTool) {
    this.canvas_.setTool(tool);
  }

  flush() {
    return this.canvas_.flush();
  }

  setOutOfBoundsColor(hexColor: string) {
    this.canvas_.setOutOfBoundsColor(hexColor);
  }

  setBorderImage(url: string) {
    this.canvas_.setBorderImage(url);
  }

  /** @param spacing in points */
  setPageSpacing(spacing: number) {
    this.canvas_.setVerticalPageLayout(spacing);
  }

  dispatchPointerEvent(evt: drawings.InputEvent) {
    this.canvas_.dispatchInput(evt);
  }

  undo() {
    this.canvas_.undo();
  }

  redo() {
    this.canvas_.redo();
  }
}

window.initInk = async function() {
  const canvas = await drawings.Canvas.execute(document.body);
  return new InkApi(canvas);
};
