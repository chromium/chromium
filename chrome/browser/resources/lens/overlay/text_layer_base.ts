// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {GestureEvent} from './selection_utils.js';

export type TextCopyCallback =
    (textStartIndex: number, textEndIndex: number, text: string) => void;

// Shared interface for the text layer on the overlay.
export interface TextLayerBase {
  // Mouse events that the text selection layer handles.
  handleRightClick(event: PointerEvent): boolean;
  handleGestureStart(event: GestureEvent): boolean;
  handleGestureDrag(event: GestureEvent): void;
  handleGestureEnd(): void;
  cancelGesture(): void;

  // Called when a new selection was started regardless of which layer handled
  // it. This includes region and text selections.
  onSelectionStart(): void;

  // Called when a selection was finished regardless of which layer handled it.
  // This includes region and text selections.
  onSelectionFinish(): void;

  // Called when the selection overlay attempts to copy detected text.
  onCopyDetectedText(
      startIndex: number, endIndex: number, callbackFn: TextCopyCallback): void;

  selectAndSendWords(selectionStartIndex: number, selectionEndIndex: number):
      void;
  selectAndTranslateWords(
      selectionStartIndex: number, selectionEndIndex: number): void;

  getElementForTesting(): Element;
}
