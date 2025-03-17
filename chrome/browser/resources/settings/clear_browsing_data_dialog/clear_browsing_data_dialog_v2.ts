// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-clear-browsing-data-dialog-v2' allows the user to
 * delete browsing data that has been cached by Chromium.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import '../settings_shared.css.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './clear_browsing_data_dialog_v2.html.js';

export interface SettingsClearBrowsingDataDialogV2Element {
  $: {
    deleteBrowsingDataDialog: CrDialogElement,
  };
}

const SettingsClearBrowsingDataDialogV2ElementBase = PrefsMixin(PolymerElement);

export class SettingsClearBrowsingDataDialogV2Element extends
    SettingsClearBrowsingDataDialogV2ElementBase {
  static get is() {
    return 'settings-clear-browsing-data-dialog-v2';
  }

  static get template() {
    return getTemplate();
  }

  /** Click handler for the "Cancel" button. */
  private onCancelClick_() {
    this.$.deleteBrowsingDataDialog.close();
  }

  /** Click handler for the "Delete data" button. */
  private onClearBrowsingDataClick_() {
    // TODO(crbug.com/397187800): Trigger the deletion.
  }
}

customElements.define(
    SettingsClearBrowsingDataDialogV2Element.is,
    SettingsClearBrowsingDataDialogV2Element);
