// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// API: https://drafts.css-houdini.org/css-paint-api-1/

declare namespace CSS {
  namespace paintWorklet {
    export function addModule(url: string): void;
  }
}

declare class PaintWorkletGlobalScope {
  registerPaint(name: string, paintCtor: PaintInstanceConstructor): void;
  readonly devicePixelRatio: number;
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
