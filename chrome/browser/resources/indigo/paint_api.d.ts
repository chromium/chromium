// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Typings for the parts of the CSS Painting API used here, which TypeScript's
// DOM library does not include.
// API: https://drafts.css-houdini.org/css-paint-api-1/

declare namespace CSS {
  const paintWorklet: Worklet;
}

declare function registerPaint(
    name: string, paintCtor: PaintInstanceConstructor): void;

declare class PaintSize {
  readonly width: number;
  readonly height: number;
}

interface PaintInstanceConstructor {
  new(): {
    paint(
        ctx: PaintRenderingContext2D,
        size: PaintSize,
        properties: StylePropertyMapReadOnly,
        ): void,
  };
}

interface PaintRenderingContext2D extends CanvasState, CanvasTransform,
                                          CanvasCompositing,
                                          CanvasImageSmoothing,
                                          CanvasFillStrokeStyles,
                                          CanvasShadowStyles, CanvasRect,
                                          CanvasDrawPath, CanvasDrawImage,
                                          CanvasPathDrawingStyles, CanvasPath {}
