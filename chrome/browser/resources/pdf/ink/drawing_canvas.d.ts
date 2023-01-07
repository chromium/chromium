// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Definitions for export-safe ink.Canvas wrapper.
 *
 * This file defines types and an interface, drawings.Canvas, that are safe for
 * export and satisfy the usage in the Chrome PDF annotation mode.
 *
 * See
 * https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/resources/pdf/ink/ink_api.js
 * for usage.
 */

declare namespace drawings {
  /**
   * A simple rectangle type.  The coordinate system depends on context.
   */
  export interface Box {
    left: number;
    top: number;
    right: number;
    bottom: number;
  }

  /**
   * A PointerEvent-like record type that can be used to pass PointerEvents or
   * an equivalent object with adjusted fields to the Ink engine.
   *
   * Ink depends on the timestamps of events and offsetX/Y, none of which can be
   * set in the synthetic PointerEvent constructor.
   */
  export interface InputEvent {
    type: string;
    timeStamp: number;
    pointerType: string;
    pointerId: number;
    offsetX: number;
    offsetY: number;
    pressure: number;
    button: number;
    buttons: number;
  }

  /**
   * Tool types supported in the Ink engine.
   *
   * See http://go/ink-tools for details on each tool.
   *
   * Note: These values map to the AnnotationTool definition in Chromium.
   */
  enum ToolType {
    MARKER = 'marker',
    PEN = 'pen',
    HIGHLIGHTER = 'highlighter',
    ERASER = 'eraser',
  }

  export interface UndoState {
    canUndo: boolean;
    canRedo: boolean;
  }

  interface ToolParams {
    tool: ToolType|string;
    color?: string;
    size?: number;
  }

  /**
   * The main interface to Ink.
   *
   * Many of these functions ultimately delegate to C++ compiled to WebAssembly.
   * The WebAssembly module depends on global state, thus it is only safe to
   * have one instance at a time.
   *
   * Use the static execute function to get an instance of the class.
   */
  export interface Canvas {
    /**
     * Notifies the attached listener when the undo/redo state changes.
     */
    addUndoRedoListener(listener: (undoState: UndoState) => void): void;

    /**
     * Sets the PDF to edit in the Ink engine.  This resets any existing
     * annotations, the camera, and any other state in the engine.
     */
    setPDF(buffer: ArrayBuffer|Uint8Array): Promise<void>;

    /**
     * Returns a copy of the currently-edited PDF with the Ink annotations
     * serialized into it.
     */
    getPDF(): Promise<Uint8Array>;

    /**
     * Returns the currently-edited PDF with the Ink annotations serialized into
     * it.  This destructively moves the PDF out of the Ink engine, and the
     * engine should not be issued any further strokes or functions calls until
     * setPDF is called again.
     */
    getPDFDestructive(): Promise<Uint8Array>;

    /**
     * Set the camera to the provided box in PDF coordinates.
     */
    setCamera(camera: Box): void;

    /**
     * Set the tool parameters in the Ink engine.
     *
     * See AnnotationTool in Chromium.
     */
    setTool({tool, color, size}: ToolParams): void;

    /**
     * Returns a Promise that is resolved when all previous asynchronous engine
     * operations are completed.  Use this prior to calling getPDF to ensure all
     * Ink strokes are serialized.
     */
    flush(): Promise<void>;

    /**
     * Returns a Promise that is resolved when no new frames will be requested.
     */
    waitForZeroFps(): Promise<void>;

    /**
     * Set the out of bounds color drawn around the PDF and between pages.
     */
    setOutOfBoundsColor(color: string): void;

    /**
     * Set the image used as the page border around each page.  Must be a data
     * or Object URL to a nine-patch PNG image.
     */
    setBorderImage(url: string): void;

    /**
     * Set the page layout to vertical (default) and the given spacing.
     */
    setVerticalPageLayout(spacing: number): void;

    /**
     * Dispatch the given PointerEvent-like to the Ink engine.
     *
     * This will cause the engine to draw a line, erase, etc. depending on the
     * current tool parameters.  Note that the Ink engine depends on the
     * timestamp when processing events, but synthetic PointerEvents have the
     * timestamp set to the creation time and can't be changed.  Instead
     * dispatch an object literal with the same fields with the desired
     * timestamp.
     */
    dispatchInput(event: InputEvent): void;

    /** Undo the most recent operation. */
    undo(): void;

    /** Redo the last undone operation. */
    redo(): void;
  }

  // Workaround for TypeScript's lack of support on static methods on
  // interfaces. https://github.com/microsoft/TypeScript/issues/13462
  export namespace Canvas {
    /**
     * Factory function for the Canvas instance.
     *
     * @param parent Element to render the canvas into.
     */
    function execute(parent: Element): Promise<Canvas>;
  }
}
