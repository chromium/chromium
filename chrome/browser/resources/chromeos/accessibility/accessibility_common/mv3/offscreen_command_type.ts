// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestImportManager} from '/common/testing/test_import_manager.js';

/**
 * The types of commands that can be sent between the offscreen document and the
 * Accessibility Common service workers.
 */
export enum OffscreenCommandType {
  // From service worker to offscreen document:
  DICTATION_PLAY_CANCEL = 'DictationPlayCancel',
  DICTATION_PLAY_START = 'DictationPlayStart',
  DICTATION_PLAY_END = 'DictationPlayEnd',
  DICTATION_PUMPKIN_INSTALL = 'DictationPumpkinInstall',
  DICTATION_PUMPKIN_RECEIVE = 'DictationPumpkinReceive',
  DICTATION_PUMPKIN_SEND = 'DictationPumpkinSend',
  FACEGAZE_WEBCAM_DETECT_LANDMARK = 'FacegazeWebCamDetectLandmark',
  FACEGAZE_WEBCAM_INITIALIZE = 'FacegazeWebCamInitialize',
  FACEGAZE_WEBCAM_STOP = 'FacegazeWebCamStop',
  // Test-only:
  FACEGAZE_MOCK_NO_CAMERA_FOR_TEST = 'FacegzeMockNoCameraFortest',
  FACEGAZE_MOCK_TIMEOUT_FOR_TEST = 'FacegzeMockTimeoutFortest',
  FACEGAZE_MOCK_RUN_LATEST_TIMEOUT_FOR_TEST = 'FacegzeRunLatestTimeoutFortest',
  FACEGAZE_CONNECT_TO_WEB_CAM_FOR_TEST = 'FacegzeConnectToWebCamFortest',
  FACEGAZE_GET_CAMERA_RETRIES_FOR_TEST = 'FacegzeGetCameraRetriesFortest',
  FACEGAZE_SET_CAMERA_RETRIES_FOR_TEST = 'FacegzeSetCameraRetriesFortest',

  // From offscreen document to service worker:
  FACEGAZE_SW_INSTALL_ASSETS = 'FacegazeSwInstallAssets',
  FACEGAZE_SW_ON_TRACK_MUTED = 'FacegazeSwOnTrackMuted',
  FACEGAZE_SW_ON_TRACK_UNMUTED = 'FacegazeSwOnTrackUnmuted',
  FACEGAZE_SW_SET_PREF = 'FacegazeSwSetPref',
  FACEGAZE_SW_UPDATE_BUBBLE_REMAINING_RETRIES =
      'FacegazeSwUpdateBubbleRemainingRetries',
  MESSENGER_SW_READY = 'MessengerSwReady',
}

TestImportManager.exportForTesting(
    ['OffscreenCommandType', OffscreenCommandType]);
