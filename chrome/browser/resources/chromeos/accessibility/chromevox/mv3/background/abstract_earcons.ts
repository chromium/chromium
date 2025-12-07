// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {LocalStorage} from '/common/local_storage.js';
import {TestImportManager} from '/common/testing/test_import_manager.js';

import type {EarconId} from '../common/earcon_id.js';

type Rect = chrome.automation.Rect;

/**
 * Base class for implementing earcons.
 * When adding earcons, please add them to chromevox/mv3/common/earcon_id.js.
 */
export abstract class AbstractEarcons {
  /**
   * Plays the specified earcon sound.
   * @param earcon An earcon identifier.
   * @param opt_location A location associated with
   *     the earcon such as a control's bounding rectangle.
   */
  abstract playEarcon(earcon: EarconId, opt_location?: Rect): void;

  /**
   * Cancels the specified earcon sound.
   * @param earcon An earcon identifier.
   */
  abstract cancelEarcon(earcon: EarconId): void;

  /**
   * Whether or not earcons are available.
   * @return True if earcons are available.
   */
  earconsAvailable(): boolean {
    return true;
  }

  /**
   * Whether or not earcons are enabled.
   * @return True if earcons are enabled.
   */
  get enabled(): boolean {
    return LocalStorage.getBoolean('earcons');
  }

  /**
   * Set whether or not earcons are enabled.
   * @param value True turns on earcons, false turns off earcons.
   */
  set enabled(value: boolean) {
    LocalStorage.set('earcons', value);
  }

  /**
   * Toggle the current enabled state and announces the new state to the user.
   */
  abstract toggle(): void;
}

TestImportManager.exportForTesting(AbstractEarcons);
