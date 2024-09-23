// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FaceGazeConstants} from './constants.js';

/** Allows developers/testers to customize FaceGaze weights using the UI. */
export class Weights {
  declare private weights_: Map<string, number>;

  constructor() {
    this.weights_ = new Map();
  }

  /**
   * This is a placeholder UI that allows devs and dogfooders to adjust what
   * type of facial tracking works best for them.
   */
  addListenersToWeights(): void {
    const listener = (): void => {
      let forehead =
          (document.getElementById('foreheadWeight') as HTMLInputElement)
              .valueAsNumber;
      let foreheadTop =
          (document.getElementById('foreheadTopWeight') as HTMLInputElement)
              .valueAsNumber;
      let noseTip =
          (document.getElementById('noseTipWeight') as HTMLInputElement)
              .valueAsNumber;
      let leftTemple =
          (document.getElementById('leftTempleWeight') as HTMLInputElement)
              .valueAsNumber;
      let rightTemple =
          (document.getElementById('rightTempleWeight') as HTMLInputElement)
              .valueAsNumber;
      let rotation =
          (document.getElementById('rotationWeight') as HTMLInputElement)
              .valueAsNumber;
      const sum = forehead + foreheadTop + noseTip + leftTemple + rightTemple +
          rotation;
      forehead /= sum;
      foreheadTop /= sum;
      noseTip /= sum;
      leftTemple /= sum;
      rightTemple /= sum;
      rotation /= sum;
      const weights =
          {forehead, foreheadTop, noseTip, leftTemple, rightTemple, rotation};
      this.weights_ = new Map(Object.entries(weights));
      chrome.runtime.sendMessage(
          undefined,
          {type: FaceGazeConstants.UPDATE_LANDMARK_WEIGHTS, weights});
    };
    document.getElementById('foreheadWeight')!.addEventListener(
        'input', listener);
    document.getElementById('foreheadTopWeight')!.addEventListener(
        'input', listener);
    document.getElementById('noseTipWeight')!.addEventListener(
        'input', listener);
    document.getElementById('leftTempleWeight')!.addEventListener(
        'input', listener);
    document.getElementById('rightTempleWeight')!.addEventListener(
        'input', listener);
    document.getElementById('rotationWeight')!.addEventListener(
        'input', listener);
  }
}

declare global {
  var weights: Weights;
}

document.addEventListener('DOMContentLoaded', () => {
  globalThis.weights = new Weights();
  globalThis.weights.addListenersToWeights();
});
