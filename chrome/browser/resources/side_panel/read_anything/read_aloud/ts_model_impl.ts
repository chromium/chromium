// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ReadAloudModelBrowserProxy} from './read_aloud_model_browser_proxy.js';
import type {Segment} from './read_aloud_types.js';
import {TextSegmenter} from './text_segmenter.js';


// Read aloud model implementation based in TS to be used when the
// ReadAnythingReadAloudTSTextSegmentation flag is enabled.
export class TsReadModelImpl implements ReadAloudModelBrowserProxy {
  // TODO: crbug.com/440400392- Implement all of the ReadAloudModelBrowserProxy
  // methods.
  getHighlightForCurrentSegmentIndex(): Segment[] {
    return [];
  }

  getCurrentTextSegments(): Segment[] {
    return [];
  }

  getCurrentTextContent(): string {
    return '';
  }

  getAccessibleText(text: string, maxSpeechLength: number): string {
    return text.slice(
        0,
        TextSegmenter.getInstance().getAccessibleBoundary(
            text, maxSpeechLength));
  }

  resetSpeechToBeginning(): void {
    return;
  }

  moveSpeechForward(): void {
    return;
  }

  moveSpeechBackwards(): void {
    return;
  }

  isInitialized(): boolean {
    return false;
  }

  init(): void {
    return;
  }
}
