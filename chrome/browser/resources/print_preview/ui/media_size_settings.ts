// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './print_preview_shared.css.js';
import './settings_section.js';
import './settings_select.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {MediaSizeCapability} from '../data/cdd.js';

import {getTemplate} from './media_size_settings.html.js';
import {SettingsMixin} from './settings_mixin.js';

const PrintPreviewMediaSizeSettingsElementBase = SettingsMixin(PolymerElement);

export class PrintPreviewMediaSizeSettingsElement extends
    PrintPreviewMediaSizeSettingsElementBase {
  static get is() {
    return 'print-preview-media-size-settings';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      capability: Object,

      disabled: Boolean,
    };
  }

  capability: MediaSizeCapability;
  disabled: boolean;

  static get observers() {
    return [
      'onMediaSizeSettingChange_(settings.mediaSize.*, capability.option)',
    ];
  }

  private onMediaSizeSettingChange_() {
    if (!this.capability) {
      return;
    }
    const valueToSet = JSON.stringify(this.getSettingValue('mediaSize'));
    for (const option of this.capability.option) {
      if (JSON.stringify(option) === valueToSet) {
        this.shadowRoot!.querySelector('print-preview-settings-select')!
            .selectValue(valueToSet);
        return;
      }
    }

    const defaultOption = this.capability.option.find(o => !!o.is_default) ||
        this.capability.option[0];
    this.setSetting('mediaSize', defaultOption);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'print-preview-media-size-settings': PrintPreviewMediaSizeSettingsElement;
  }
}

customElements.define(
    PrintPreviewMediaSizeSettingsElement.is,
    PrintPreviewMediaSizeSettingsElement);
