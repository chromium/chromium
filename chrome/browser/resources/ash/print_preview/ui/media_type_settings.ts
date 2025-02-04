// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './print_preview_shared.css.js';
import './settings_section.js';
import '/strings.m.js';
import './settings_select.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {MediaTypeCapability, MediaTypeOption, SelectOption} from '../data/cdd.js';

import {getTemplate} from './media_type_settings.html.js';
import {SettingsMixin} from './settings_mixin.js';

type LabelledMediaTypeOption = MediaTypeOption&SelectOption;
export interface LabelledMediaTypeCapability {
  option: LabelledMediaTypeOption[];
}

const PrintPreviewMediaTypeSettingsElementBase = SettingsMixin(PolymerElement);

export class PrintPreviewMediaTypeSettingsElement extends
    PrintPreviewMediaTypeSettingsElementBase {
  static get is() {
    return 'print-preview-media-type-settings';
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

  static get observers() {
    return [
      'onMediaTypeSettingChange_(settings.mediaType.*, capability.option)',
    ];
  }

  capability: MediaTypeCapability;
  disabled: boolean;
  private lastSelectedValue_: string = '';

  private onMediaTypeSettingChange_() : void {
    if (!this.capability) {
      return;
    }
    const valueToSet = JSON.stringify(this.getSettingValue('mediaType'));
    for (const option of this.capability.option) {
      if (JSON.stringify(option) === valueToSet) {
        this.shadowRoot!.querySelector('print-preview-settings-select')!
            .selectValue(valueToSet);
        this.lastSelectedValue_ = valueToSet;
        return;
      }
    }

    // If the sticky settings are not compatible with the initially selected
    // printer, reset this setting to the printer default. Only do this when
    // the setting changes, as occurs for sticky settings, and not for a printer
    // change which can also trigger this observer. The model is responsible for
    // setting a compatible media size value after printer changes.
    if (valueToSet !== this.lastSelectedValue_) {
      const defaultOption = this.capability.option.find(o => !!o.is_default) ||
          this.capability.option[0];
      this.setSetting('mediaType', defaultOption, /*noSticky=*/ true);
    }
  }

  private isSelectionBoxDisabled_(): boolean {
    return this.disabled || this.getSetting('mediaType').setByDestinationPolicy;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'print-preview-media-type-settings': PrintPreviewMediaTypeSettingsElement;
  }
}

customElements.define(
    PrintPreviewMediaTypeSettingsElement.is,
    PrintPreviewMediaTypeSettingsElement);
