// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FaceLandmarkerResult} from '../third_party/mediapipe/task_vision/vision.js';

/** The facial gestures that are supported by FaceGaze. */
export enum FacialGesture {
  BROW_DOWN_LEFT = 'browDownLeft',
  BROW_DOWN_RIGHT = 'browDownRight',
  BROW_INNER_UP = 'browInnerUp',
  JAW_OPEN = 'jawOpen',
  MOUTH_LEFT = 'mouthLeft',
  MOUTH_RIGHT = 'mouthRight',
}

export class GestureDetector {
  /**
   * Computes which FacialGestures were detected. Note that this will only
   * return a gesture if it is specified in `confidenceMap`, as this function
   * uses the confidence to decide whether or not to include the gesture in
   * the final result.
   */
  static detect(
      result: FaceLandmarkerResult,
      confidenceMap: Map<FacialGesture, number>): FacialGesture[] {
    const gestures = [];
    for (const classification of result.faceBlendshapes) {
      for (const category of classification.categories) {
        const gesture = category.categoryName as FacialGesture;
        const confidence = confidenceMap.get(gesture);
        if (confidence === undefined) {
          continue;
        }

        if (category.score < confidence) {
          continue;
        }

        gestures.push(gesture);
      }
    }
    return gestures;
  }
}
