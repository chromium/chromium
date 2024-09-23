// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {LocalStorage} from '/common/local_storage.js';
import {TestImportManager} from '/common/testing/test_import_manager.js';

import {EarconId} from '../common/earcon_id.js';

/**
 * Base class for implementing earcons.
 * When adding earcons, please add them to chromevox/common/earcon_id.js.
 */
export class AbstractEarcons {
  /**
   * Plays the specified earcon sound.
   * @param {EarconId} earcon An earcon identifier.
   * @param {chrome.automation.Rect=} opt_location A location associated with
   *     the earcon such as a control's bounding rectangle.
   */
  playEarcon(earcon, opt_location) {}

  /**
   * Cancels the specified earcon sound.
   * @param {EarconId} earcon An earcon identifier.
   */
  cancelEarcon(earcon) {}

  /**
   * Whether or not earcons are available.
   * @return {boolean} True if earcons are available.
   */
  earconsAvailable() {
    return true;
  }

  /**
   * Whether or not earcons are enabled.
   * @return {boolean} True if earcons are enabled.
   */
  get enabled() {
    return LocalStorage.getBoolean('earcons');
  }

  /**
   * Set whether or not earcons are enabled.
   * @param {boolean} value True turns on earcons, false turns off earcons.
   */
  set enabled(value) {
    LocalStorage.set('earcons', value);
  }

  /**
   * Toggle the current enabled state and announces the new state to the user.
   */
  toggle() {}
}

TestImportManager.exportForTesting(AbstractEarcons);
