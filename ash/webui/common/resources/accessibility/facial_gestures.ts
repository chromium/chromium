// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The facial gestures that are supported by FaceGaze. New values should also
 * be added to FacialGesturesToMediapipeGestures in
 * facegaze/gesture_detector.ts, FacialGesture definition in
 * accessibility_private.json, and ConvertFacialGestureType in
 * accessibility_extension_api_ash.cc. Please keep alphabetical.
 */
export enum FacialGesture {
  BROW_INNER_UP = 'browInnerUp',
  BROWS_DOWN = 'browsDown',
  EYE_SQUINT_LEFT = 'eyeSquintLeft',
  EYE_SQUINT_RIGHT = 'eyeSquintRight',
  EYES_BLINK = 'eyesBlink',
  EYES_LOOK_DOWN = 'eyesLookDown',
  EYES_LOOK_LEFT = 'eyesLookLeft',
  EYES_LOOK_RIGHT = 'eyesLookRight',
  EYES_LOOK_UP = 'eyesLookUp',
  JAW_LEFT = 'jawLeft',
  JAW_OPEN = 'jawOpen',
  JAW_RIGHT = 'jawRight',
  MOUTH_FUNNEL = 'mouthFunnel',
  MOUTH_LEFT = 'mouthLeft',
  MOUTH_PUCKER = 'mouthPucker',
  MOUTH_RIGHT = 'mouthRight',
  MOUTH_SMILE = 'mouthSmile',
  MOUTH_UPPER_UP = 'mouthUpperUp',
}
