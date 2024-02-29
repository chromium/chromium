// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestImportManager} from '/common/testing/test_import_manager.js';
import type {FaceLandmarkerResult} from '/third_party/mediapipe/vision.js';

/**
 * The facial gestures that are supported by FaceGaze. New values should also
 * be added to FacialGesturesToMediapipeGestures.
 * Note that these correspond to values in
 * facegaze_facial_expression_subpage.ts, and if these values get changed, those
 * should too.
 */
export enum FacialGesture {
  BROWS_DOWN = 'browsDown',
  BROW_INNER_UP = 'browInnerUp',
  EYES_LOOK_DOWN = 'eyesLookDown',
  EYES_LOOK_UP = 'eyesLookUp',
  JAW_OPEN = 'jawOpen',
  MOUTH_LEFT = 'mouthLeft',
  MOUTH_PUCKER = 'mouthPucker',
  MOUTH_RIGHT = 'mouthRight',
}

/** Facial gestures recognized by Mediapipe. */
export enum MediapipeFacialGesture {
  BROW_DOWN_LEFT = 'browDownLeft',
  BROW_DOWN_RIGHT = 'browDownRight',
  BROW_INNER_UP = 'browInnerUp',
  EYE_LOOK_DOWN_LEFT = 'eyeLookDownLeft',
  EYE_LOOK_DOWN_RIGHT = 'eyeLookDownRight',
  EYE_LOOK_UP_LEFT = 'eyeLookUpLeft',
  EYE_LOOK_UP_RIGHT = 'eyeLookUpRight',
  JAW_OPEN = 'jawOpen',
  MOUTH_LEFT = 'mouthLeft',
  MOUTH_PUCKER = 'mouthPucker',
  MOUTH_RIGHT = 'mouthRight',
}

/**
 * Mapping of gestures supported by FaceGaze to mediapipe gestures; allows for
 * compound gestures.
 */
export const FacialGesturesToMediapipeGestures = new Map([
  [
    FacialGesture.BROWS_DOWN,
    [
      MediapipeFacialGesture.BROW_DOWN_LEFT,
      MediapipeFacialGesture.BROW_DOWN_RIGHT,
    ],
  ],
  [FacialGesture.BROW_INNER_UP, [MediapipeFacialGesture.BROW_INNER_UP]],
  [FacialGesture.JAW_OPEN, [MediapipeFacialGesture.JAW_OPEN]],
  [FacialGesture.MOUTH_LEFT, [MediapipeFacialGesture.MOUTH_LEFT]],
  [FacialGesture.MOUTH_RIGHT, [MediapipeFacialGesture.MOUTH_RIGHT]],
  [FacialGesture.MOUTH_PUCKER, [MediapipeFacialGesture.MOUTH_PUCKER]],
  [
    FacialGesture.EYES_LOOK_DOWN,
    [
      MediapipeFacialGesture.EYE_LOOK_DOWN_LEFT,
      MediapipeFacialGesture.EYE_LOOK_DOWN_RIGHT,
    ],
  ],
  [
    FacialGesture.EYES_LOOK_UP,
    [
      MediapipeFacialGesture.EYE_LOOK_UP_LEFT,
      MediapipeFacialGesture.EYE_LOOK_UP_RIGHT,
    ],
  ],
]);

export class GestureDetector {
  private static mediapipeFacialGestureSet_ =
      new Set(Object.values(MediapipeFacialGesture));

  /**
   * Computes which FacialGestures were detected. Note that this will only
   * return a gesture if it is specified in `confidenceMap`, as this function
   * uses the confidence to decide whether or not to include the gesture in
   * the final result.
   */
  static detect(
      result: FaceLandmarkerResult,
      confidenceMap: Map<FacialGesture, number>): FacialGesture[] {
    // Look through the blendshapes to find the gestures from mediapipe that we
    // care about.
    const recognizedGestures = new Map();
    for (const classification of result.faceBlendshapes) {
      for (const category of classification.categories) {
        const gesture = category.categoryName as MediapipeFacialGesture;
        if (GestureDetector.mediapipeFacialGestureSet_.has(gesture)) {
          recognizedGestures.set(gesture, category.score);
        }
      }
    }

    if (recognizedGestures.size === 0) {
      return [];
    }

    // Look through the facial gestures to see which were detected by mediapipe.
    const gestures: FacialGesture[] = [];
    for (const [faceGazeGesture, mediapipeGestures] of
             FacialGesturesToMediapipeGestures) {
      const confidence = confidenceMap.get(faceGazeGesture);
      if (confidence === undefined) {
        // This gesture isn't in-use by FaceGaze at the moment.
        continue;
      }

      // Score will be the minimum from among the compound gestures.
      let score = -1;
      let hasCompoundGesture = true;
      for (const mediapipeGesture of mediapipeGestures) {
        if (!recognizedGestures.has(mediapipeGesture)) {
          hasCompoundGesture = false;
          break;
        }
        // The score of a compound gesture is the maximum of its component
        // parts. This is max instead of min in case people have uneven
        // facial strength or dexterity.
        score = Math.max(score, recognizedGestures.get(mediapipeGesture));
      }
      if (!hasCompoundGesture) {
        continue;
      }

      if (score < confidence) {
        continue;
      }

      gestures.push(faceGazeGesture);
    }
    return gestures;
  }
}

TestImportManager.exportForTesting(
    ['FacialGesture', FacialGesture],
    ['MediapipeFacialGesture', MediapipeFacialGesture],
    ['FacialGesturesToMediapipeGestures', FacialGesturesToMediapipeGestures]);
