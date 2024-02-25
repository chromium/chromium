// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Interface to prevent circular dependencies between
 * CommandHandler and GestureCommandHandler.
 */
import {GestureGranularity} from '../../common/gesture_command_data.js';

export const GestureInterface = {};

/** @return {GestureGranularity} */
GestureInterface.getGranularity = function() {
  if (GestureInterface.granularityGetter) {
    return GestureInterface.granularityGetter();
  } else {
    throw new Error('GestureInterface not initialized before access.');
  }
};

/** @param {GestureGranularity} granularity */
GestureInterface.setGranularity = function(granularity) {
  if (GestureInterface.granularitySetter) {
    GestureInterface.granularitySetter(granularity);
  } else {
    throw new Error('GestureInterface not initialized before setting a value.');
  }
};

/** @public {?function(): GestureGranularity} */
GestureInterface.granularityGetter = null;

/** @public {?function(GestureGranularity)} */
GestureInterface.granularitySetter = null;
