// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Processes events related to editing text and emits the
 * appropriate spoken and braille feedback.
 */
import {CursorRange} from '/common/cursors/range.js';

import {SettingsManager} from '../../common/settings_manager.js';
import {BrailleTranslatorManager} from '../braille/braille_translator_manager.js';
import {ChromeVoxRange, ChromeVoxRangeObserver} from '../chromevox_range.js';
import {ChromeVoxState} from '../chromevox_state.js';

/**
 * An observer that reacts to ChromeVox range changes that modifies braille
 * table output when over email or url text fields.
 */
export class EditingRangeObserver implements ChromeVoxRangeObserver {
  static instance?: ChromeVoxRangeObserver;

  constructor() {
    ChromeVoxState.ready().then(() => ChromeVoxRange.addObserver(this));
  }

  static init(): void {
    if (EditingRangeObserver.instance) {
      throw new Error('Cannot call EditingRangeObserver.init more than once');
    }
    EditingRangeObserver.instance = new EditingRangeObserver();
  }

  onCurrentRangeChanged(
      range: CursorRange | null,_fromEditing?: boolean): void {
    const inputType = range && range.start.node.inputType;
    if (inputType === 'email' || inputType === 'url') {
      BrailleTranslatorManager.instance.refresh(
          SettingsManager.getString('brailleTable8'));
      return;
    }
    BrailleTranslatorManager.instance.refresh(
        SettingsManager.getString('brailleTable'));
  }
}
