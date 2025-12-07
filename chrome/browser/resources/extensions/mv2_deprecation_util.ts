// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotReached} from 'chrome://resources/js/assert.js';

// The current stage of the Manifest V2 deprecation.
// IMPORTANT: this should follow the same order as MV2ExperimentStage in
// chrome/browser/extensions/mv2_experiment_stage.h
export enum Mv2ExperimentStage {
  NONE = 0,
  WARNING = 1,
  DISABLE_WITH_REENABLE = 2,
  UNSUPPORTED = 3
}

export function getMv2ExperimentStage(stage: number): Mv2ExperimentStage {
  if (stage === 0) {
    return Mv2ExperimentStage.NONE;
  }

  if (stage === 1) {
    return Mv2ExperimentStage.WARNING;
  }

  if (stage === 2) {
    return Mv2ExperimentStage.DISABLE_WITH_REENABLE;
  }

  if (stage === 3) {
    return Mv2ExperimentStage.UNSUPPORTED;
  }

  assertNotReached();
}
