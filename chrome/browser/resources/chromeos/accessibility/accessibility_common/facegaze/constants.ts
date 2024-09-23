// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestImportManager} from '/common/testing/test_import_manager.js';

export namespace FaceGazeConstants {
  // The message type for an event that is sent from the weights.html page to
  // the background page.
  export const UPDATE_LANDMARK_WEIGHTS = 'updateLandmarkWeights';
}

TestImportManager.exportForTesting(['FaceGazeConstants', FaceGazeConstants]);
