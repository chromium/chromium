// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview @externs for export-safe ink.Canvas wrapper.
 *
 * This file defines types and an interface, drawings.Canvas, that are safe for
 * export and satisfy the usage in the Chrome PDF annotation mode.
 */

/** @const Namespace */
const drawings = {};

/**
 * A simple rectangle type.  The coordinate system depends on context.
 *
 * @typedef {{left: number, top: number, right: number, bottom: number}}
 */
drawings.Box;

/**
 * A PointerEvent-like record type that can be used to pass PointerEvents or an
 * equivalent object with adjusted fields to the Ink engine.
 *
 * Ink depends on the timestamps of events and offsetX/Y, none of which can be
 * set in the synthetic PointerEvent constructor.
 *
 * @typedef {{
 *   type: string,
 *   timeStamp: number,
 *   pointerType: string,
 *   pointerId: number,
 *   offsetX: number,
 *   offsetY: number,
 *   pressure: number,
 *   button: number,
 *   buttons: number,
 * }}
 */
drawings.InputEvent;

/**
 * Tool types supported in the Ink engine.
 *
 * See http://go/ink-tools for details on each tool.
 *
 * Note: These values map to the AnnotationTool definition in Chromium.
 *
 * @enum {string}
 */
drawings.ToolType = {
  MARKER: 'marker',
  PEN: 'pen',
  HIGHLIGHTER: 'highlighter',
  ERASER: 'eraser',
};

/** @typedef {{canUndo: boolean, canRedo: boolean}} */
drawings.UndoState;

/**
 * The main interface to Ink.
 *
 * Many of these functions ultimately delegate to C++ compiled to WebAssembly.
 * The WebAssembly module depends on global state, thus it is only safe to have
 * one instance at a time.
 *
 * Use the static execute function to get an instance of the class.
 *
 * @interface
 */
drawings.Canvas = class {
  /**
   * Factory function for the Canvas instance.
   *
   * @param {!Element} parent Element to render the canvas into.
   * @return {!Promise<!drawings.Canvas>}
   */
  static async execute(parent) {}

  /**
   * Notifies the attached listener when the undo/redo state changes.
   *
   * @param {function(!drawings.UndoState):undefined} listener
   */
  addUndoRedoListener(listener) {}

  /**
   * Sets the PDF to edit in the Ink engine.  This resets any existing
   * annotations, the camera, and any other state in the engine.
   *
   * @param {!ArrayBuffer|!Uint8Array} buffer
   * @return {!Promise<undefined>}
   */
  async setPDF(buffer) {}

  /**
   * Returns a copy of the currently-edited PDF with the Ink annotations
   * serialized into it.
   *
   * @return {!Uint8Array}
   */
  getPDF() {}

  /**
   * Returns the currently-edited PDF with the Ink annotations serialized into
   * it.  This destructively moves the PDF out of the Ink engine, and the engine
   * should not be issued any further strokes or functions calls until setPDF is
   * called again.
   *
   * @return {!Uint8Array}
   */
  getPDFDestructive() {}

  /**
   * Set the camera to the provided box in PDF coordinates.
   *
   * @param {!drawings.Box} camera
   */
  setCamera(camera) {}

  /**
   * Set the tool parameters in the Ink engine.
   *
   * See AnnotationTool in Chromium.
   *
   * @param {{
   *   tool: (!drawings.ToolType|string),
   *   color: (string|undefined),
   *   size: (number|undefined),
   * }} toolParams
   */
  setTool({tool, color, size}) {}

  /**
   * Returns a Promise that is resolved when all previous asynchronous engine
   * operations are completed.  Use this prior to calling getPDF to ensure all
   * Ink strokes are serialized.
   *
   * @return {!Promise<undefined>}
   */
  flush() {}

  /**
   * Set the out of bounds color drawn around the PDF and between pages.
   *
   * @param {string} color
   */
  setOutOfBoundsColor(color) {}

  /**
   * Set the image used as the page border around each page.  Must be a data or
   * Object URL to a nine-patch PNG image.
   *
   * @param {string} url
   */
  setBorderImage(url) {}

  /**
   * Set the page layout to vertical (default) and the given spacing.
   *
   * @param {number} spacing
   */
  setVerticalPageLayout(spacing) {}

  /**
   * Dispatch the given PointerEvent-like to the Ink engine.
   *
   * This will cause the engine to draw a line, erase, etc. depending on the
   * current tool parameters.  Note that the Ink engine depends on the timestamp
   * when processing events, but synthetic PointerEvents have the timestamp set
   * to the creation time and can't be changed.  Instead dispatch an object
   * literal with the same fields with the desired timestamp.
   *
   * @param {!drawings.InputEvent} event
   */
  dispatchInput(event) {}

  /** Undo the most recent operation. */
  undo() {}

  /** Redo the last undone operation. */
  redo() {}
};
