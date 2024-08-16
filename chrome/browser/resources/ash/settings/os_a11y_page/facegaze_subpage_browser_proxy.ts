// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export interface FaceGazeSubpageBrowserProxy {
  /**
   * Tells FaceGaze that the action settings is requesting information about
   * detected gestures.
   */
  toggleGestureInfoForSettings(enabled: boolean): void;
}

let instance: FaceGazeSubpageBrowserProxy|null = null;

export class FaceGazeSubpageBrowserProxyImpl implements
    FaceGazeSubpageBrowserProxy {
  static getInstance(): FaceGazeSubpageBrowserProxy {
    return instance || (instance = new FaceGazeSubpageBrowserProxyImpl());
  }

  static setInstanceForTesting(obj: FaceGazeSubpageBrowserProxy): void {
    instance = obj;
  }

  toggleGestureInfoForSettings(enabled: boolean): void {
    chrome.send('toggleGestureInfoForSettings', [enabled]);
  }
}
