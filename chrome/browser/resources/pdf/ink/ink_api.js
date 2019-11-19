// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @typedef {{
 *   canUndo: boolean,
 *   canRedo: boolean,
 * }}
 */
let UndoState;

/**
 * Wraps the Ink component with an API that can be called
 * across an IFrame boundary.
 */
class InkAPI {
  /** @param {!ink.embed.EmbedComponent} embed */
  constructor(embed) {
    this.embed_ = embed;
    this.brush_ = ink.BrushModel.getInstance(embed);
    this.camera_ = null;
  }

  /** @param {function(!UndoState)} listener */
  addUndoStateListener(listener) {
    /** @param {!ink.UndoStateChangeEvent} e */
    function wrapper(e) {
      listener({
        canUndo: e.getCanUndo(),
        canRedo: e.getCanRedo(),
      });
    }

    this.embed_.addEventListener(ink.UndoStateChangeEvent.EVENT_TYPE, wrapper);
  }

  /**
   * @param {!ArrayBuffer} buffer
   */
  setPDF(buffer) {
    // We change the type from ArrayBuffer to Uint8Array due to the consequences
    // of the buffer being passed across the iframe boundary. This realm has a
    // different ArrayBuffer constructor than `buffer`.
    // TODO(dstockwell): Update Ink to allow Uint8Array here.
    this.embed_.setPDF(
        /** @type {!ArrayBuffer} */ (
            /** @type {!*} */ (new Uint8Array(buffer))));
  }

  /**
   * @return {!Promise<Uint8Array>}
   */
  getPDF() {
    return this.embed_.getPDF();
  }

  /**
   * @return {!Uint8Array}
   */
  getPDFDestructive() {
    return this.embed_.getPDFDestructive();
  }

  async setCamera(camera) {
    this.camera_ = camera;
    this.embed_.setCamera(camera);
    // Wait for the next task to avoid a race where Ink drops the camera value
    // when the canvas is rotated in low-latency mode.
    setTimeout(() => this.embed_.setCamera(this.camera_), 0);
  }

  /** @param {AnnotationTool} tool */
  setAnnotationTool(tool) {
    const shape = {
      eraser: 'MAGIC_ERASE',
      pen: 'INKPEN',
      highlighter: 'SMART_HIGHLIGHTER_TOOL',
    }[tool.tool];
    this.brush_.setShape(shape);
    if (tool.tool != 'eraser') {
      this.brush_.setColor(/** @type {string} */ (tool.color));
    }
    this.brush_.setStrokeWidth(tool.size);
  }

  flush() {
    return new Promise(resolve => this.embed_.flush(resolve));
  }

  /** @param {string} hexColor */
  setOutOfBoundsColor(hexColor) {
    this.embed_.setOutOfBoundsColor(ink.Color.fromString(hexColor));
  }

  /** @param {string} url */
  setBorderImage(url) {
    this.embed_.setBorderImage(url);
  }

  /** @param {number} spacing in points */
  setPageSpacing(spacing) {
    this.embed_.setVerticalPageLayout(spacing);
  }

  dispatchPointerEvent(type, init) {
    const engine = document.querySelector('#ink-engine');
    const match = engine.style.transform.match(/(\d+)deg/);
    const rotation = match ? Number(match[1]) : 0;
    let offsetX = init.clientX;
    let offsetY = init.clientY;
    // If Ink's canvas has been re-orientated away from 0, we must transform
    // the event's offsetX and offsetY to correspond with the rotation and
    // offset applied.
    if ([90, 180, 270].includes(rotation)) {
      const width = window.innerWidth;
      const height = window.innerHeight;
      const matrix = new DOMMatrix();
      matrix.translateSelf(width / 2, height / 2);
      matrix.rotateSelf(0, 0, -rotation);
      matrix.translateSelf(-width / 2, -height / 2);
      const result = matrix.transformPoint({x: offsetX, y: offsetY});
      offsetX = result.x - engine.offsetLeft;
      offsetY = result.y - engine.offsetTop;
    }

    const event = new PointerEvent(type, init);
    // Ink uses offsetX and offsetY, but we can only override them, not pass
    // as part of the init.
    Object.defineProperty(event, 'offsetX', {value: offsetX});
    Object.defineProperty(event, 'offsetY', {value: offsetY});
    engine.dispatchEvent(event);
  }

  undo() {
    this.embed_.undo();
  }

  redo() {
    this.embed_.redo();
  }
}

/** @return {Promise<InkAPI>} */
window.initInk = async function() {
  const config = new ink.embed.Config();
  const embed = await ink.embed.EmbedComponent.execute(config);
  embed.assignFlag(ink.proto.Flag.ENABLE_HOST_CAMERA_CONTROL, true);
  return new InkAPI(embed);
};
