// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'os-settings-japanese-clear-ime-data-dialog' is used to
 * manage clearing personalized data for the Japanese input decoder.
 */

import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './os_japanese_clear_ime_data_dialog.html.js';

export interface OsSettingsClearPersonalizedDataDialogElement {
  $: {
    dialog: CrDialogElement,
  };
}

export class OsSettingsClearPersonalizedDataDialogElement extends
    PolymerElement {
  static get is() {
    return 'os-settings-japanese-clear-ime-data-dialog' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {};
  }

  private onCancelButtonClick_(): void {
    this.$.dialog.close();
  }
}

customElements.define(
    OsSettingsClearPersonalizedDataDialogElement.is,
    OsSettingsClearPersonalizedDataDialogElement);

declare global {
  interface HTMLElementTagNameMap {
    [OsSettingsClearPersonalizedDataDialogElement.is]:
        OsSettingsClearPersonalizedDataDialogElement;
  }
}
