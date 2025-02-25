// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {GestureEvent} from './selection_utils.js';

// Shared interface for the text layer on the overlay.
export interface TextLayerBase {
  handleRightClick(event: PointerEvent): boolean;

  handleGestureStart(event: GestureEvent): boolean;
  handleGestureDrag(event: GestureEvent): void;
  handleGestureEnd(): void;
  cancelGesture(): void;

  selectAndSendWords(selectionStartIndex: number, selectionEndIndex: number):
      void;
  selectAndTranslateWords(
      selectionStartIndex: number, selectionEndIndex: number): void;

  getElementForTesting(): Element;
}
