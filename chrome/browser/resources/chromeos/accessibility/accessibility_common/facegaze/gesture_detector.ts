// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestImportManager} from '/common/testing/test_import_manager.js';
import type {FaceLandmarkerResult} from '/third_party/mediapipe/vision.js';

import {FacialGesture} from './facial_gestures.js';

/** Facial gestures recognized by Mediapipe. */
export enum MediapipeFacialGesture {
  BROW_DOWN_LEFT = 'browDownLeft',
  BROW_DOWN_RIGHT = 'browDownRight',
  BROW_INNER_UP = 'browInnerUp',
  EYE_BLINK_LEFT = 'eyeBlinkLeft',
  EYE_BLINK_RIGHT = 'eyeBlinkRight',
  EYE_LOOK_DOWN_LEFT = 'eyeLookDownLeft',
  EYE_LOOK_DOWN_RIGHT = 'eyeLookDownRight',
  EYE_LOOK_IN_LEFT = 'eyeLookInLeft',
  EYE_LOOK_IN_RIGHT = 'eyeLookInRight',
  EYE_LOOK_OUT_LEFT = 'eyeLookOutLeft',
  EYE_LOOK_OUT_RIGHT = 'eyeLookOutRight',
  EYE_LOOK_UP_LEFT = 'eyeLookUpLeft',
  EYE_LOOK_UP_RIGHT = 'eyeLookUpRight',
  EYE_SQUINT_LEFT = 'eyeSquintLeft',
  EYE_SQUINT_RIGHT = 'eyeSquintRight',
  JAW_LEFT = 'jawLeft',
  JAW_OPEN = 'jawOpen',
  JAW_RIGHT = 'jawRight',
  MOUTH_FUNNEL = 'mouthFunnel',
  MOUTH_LEFT = 'mouthLeft',
  MOUTH_PUCKER = 'mouthPucker',
  MOUTH_RIGHT = 'mouthRight',
  MOUTH_SMILE_LEFT = 'mouthSmileLeft',
  MOUTH_SMILE_RIGHT = 'mouthSmileRight',
  MOUTH_UPPER_UP_LEFT = 'mouthUpperUpLeft',
  MOUTH_UPPER_UP_RIGHT = 'mouthUpperUpRight',
}

/**
 * Mapping of gestures supported by FaceGaze to mediapipe gestures; allows for
 * compound gestures.
 */
export const FacialGesturesToMediapipeGestures = new Map([
  [FacialGesture.BROW_INNER_UP, [MediapipeFacialGesture.BROW_INNER_UP]],
  [
    FacialGesture.BROWS_DOWN,
    [
      MediapipeFacialGesture.BROW_DOWN_LEFT,
      MediapipeFacialGesture.BROW_DOWN_RIGHT,
    ],
  ],
  [FacialGesture.EYE_SQUINT_LEFT, [MediapipeFacialGesture.EYE_SQUINT_LEFT]],
  [FacialGesture.EYE_SQUINT_RIGHT, [MediapipeFacialGesture.EYE_SQUINT_RIGHT]],
  [
    FacialGesture.EYES_BLINK,
    [
      MediapipeFacialGesture.EYE_BLINK_LEFT,
      MediapipeFacialGesture.EYE_BLINK_RIGHT,
    ],
  ],
  [
    FacialGesture.EYES_LOOK_DOWN,
    [
      MediapipeFacialGesture.EYE_LOOK_DOWN_LEFT,
      MediapipeFacialGesture.EYE_LOOK_DOWN_RIGHT,
    ],
  ],
  [
    FacialGesture.EYES_LOOK_LEFT,
    [
      MediapipeFacialGesture.EYE_LOOK_OUT_LEFT,
      MediapipeFacialGesture.EYE_LOOK_IN_RIGHT,
    ],
  ],
  [
    FacialGesture.EYES_LOOK_RIGHT,
    [
      MediapipeFacialGesture.EYE_LOOK_OUT_RIGHT,
      MediapipeFacialGesture.EYE_LOOK_IN_LEFT,
    ],
  ],
  [
    FacialGesture.EYES_LOOK_UP,
    [
      MediapipeFacialGesture.EYE_LOOK_UP_LEFT,
      MediapipeFacialGesture.EYE_LOOK_UP_RIGHT,
    ],
  ],
  [FacialGesture.JAW_LEFT, [MediapipeFacialGesture.JAW_LEFT]],
  [FacialGesture.JAW_OPEN, [MediapipeFacialGesture.JAW_OPEN]],
  [FacialGesture.JAW_RIGHT, [MediapipeFacialGesture.JAW_RIGHT]],
  [FacialGesture.MOUTH_FUNNEL, [MediapipeFacialGesture.MOUTH_FUNNEL]],
  [FacialGesture.MOUTH_LEFT, [MediapipeFacialGesture.MOUTH_LEFT]],
  [FacialGesture.MOUTH_PUCKER, [MediapipeFacialGesture.MOUTH_PUCKER]],
  [FacialGesture.MOUTH_RIGHT, [MediapipeFacialGesture.MOUTH_RIGHT]],
  [
    FacialGesture.MOUTH_SMILE,
    [
      MediapipeFacialGesture.MOUTH_SMILE_LEFT,
      MediapipeFacialGesture.MOUTH_SMILE_RIGHT,
    ],
  ],
  [
    FacialGesture.MOUTH_UPPER_UP,
    [
      MediapipeFacialGesture.MOUTH_UPPER_UP_LEFT,
      MediapipeFacialGesture.MOUTH_UPPER_UP_RIGHT,
    ],
  ],
]);

export class GestureDetector {
  private static mediapipeFacialGestureSet_ =
      new Set(Object.values(MediapipeFacialGesture));
  declare private static shouldSendGestureDetectionInfo_: boolean;

  static toggleSendGestureDetectionInfo(enabled: boolean): void {
    this.shouldSendGestureDetectionInfo_ = enabled;
  }

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
    const gestureInfoForSettings:
        Array<{gesture: FacialGesture, confidence: number}> = [];
    for (const [faceGazeGesture, mediapipeGestures] of
             FacialGesturesToMediapipeGestures) {
      const confidence = confidenceMap.get(faceGazeGesture);
      if (!this.shouldSendGestureDetectionInfo_ && !confidence) {
        // Settings is not requesting gesture detection information and
        // this gesture is not currently used by FaceGaze.
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

      // For gestures detected with a confidence value over a threshold value of
      // 1, add the gesture and confidence value to the array of information
      // that will be sent to settings.
      if (this.shouldSendGestureDetectionInfo_ && score >= 0.01) {
        gestureInfoForSettings.push(
            {gesture: faceGazeGesture, confidence: score * 100});
      }

      if (confidence && score < confidence) {
        continue;
      }

      gestures.push(faceGazeGesture);
    }

    if (this.shouldSendGestureDetectionInfo_ &&
        gestureInfoForSettings.length > 0) {
      chrome.accessibilityPrivate.sendGestureInfoToSettings(
          gestureInfoForSettings);
    }

    return gestures;
  }
}

TestImportManager.exportForTesting(
    GestureDetector, ['FacialGesture', FacialGesture],
    ['MediapipeFacialGesture', MediapipeFacialGesture],
    ['FacialGesturesToMediapipeGestures', FacialGesturesToMediapipeGestures]);
