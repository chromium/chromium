// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Interface to prevent circular dependencies between
 * CommandHandler and GestureCommandHandler.
 */
import {GestureGranularity} from '../../common/gesture_command_data.js';

type Getter = () => GestureGranularity;
type Setter = (granularity: GestureGranularity) => void;

/** Interface to facilitate access and setting of gesture granularity. */
export class GestureInterface {
  static getGranularity(): GestureGranularity {
    if (GestureInterface.granularityGetter) {
      return GestureInterface.granularityGetter();
    } else {
      throw new Error('GestureInterface not initialized before access.');
    }
  }

  static setGranularity(granularity: GestureGranularity): void {
    if (GestureInterface.granularitySetter) {
      GestureInterface.granularitySetter(granularity);
    } else {
      throw new Error(
          'GestureInterface not initialized before setting a value.');
    }
  }

  static granularityGetter: Getter|null = null;
  static granularitySetter: Setter|null = null;
}
