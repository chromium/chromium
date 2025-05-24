// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'os-settings-japanese-clear-ime-data-dialog' is used to
 * manage clearing personalized data for the Japanese input decoder.
 */
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';

import type {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './os_japanese_clear_ime_data_dialog.html.js';
import {UserDataServiceProvider} from './user_data_service_provider.js';

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
    return {
      clearConversionHistory_: {
        type: Boolean,
      },
      clearSuggestionHistory_: {
        type: Boolean,
      },
    };
  }

  private clearConversionHistory_ = false;

  private clearSuggestionHistory_ = false;

  private onCancelButtonClick_(): void {
    this.$.dialog.close();
  }

  private async onClearButtonClick_(): Promise<void> {
    await UserDataServiceProvider.getRemote().clearJapanesePersonalizationData(
        this.clearConversionHistory_, this.clearSuggestionHistory_);
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
