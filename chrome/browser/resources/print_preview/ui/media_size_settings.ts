// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './settings_section.js';
import './settings_select.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {MediaSizeCapability} from '../data/cdd.js';

import {getCss} from './media_size_settings.css.js';
import {getHtml} from './media_size_settings.html.js';
import {SettingsMixin} from './settings_mixin.js';

const PrintPreviewMediaSizeSettingsElementBase = SettingsMixin(CrLitElement);

export class PrintPreviewMediaSizeSettingsElement extends
    PrintPreviewMediaSizeSettingsElementBase {
  static get is() {
    return 'print-preview-media-size-settings';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      capability: {type: Object},
      disabled: {type: Boolean},
    };
  }

  accessor capability: MediaSizeCapability|null = null;
  accessor disabled: boolean = false;
  private lastSelectedValue_: string = '';

  override connectedCallback() {
    super.connectedCallback();

    this.addSettingObserver('mediaSize.*', () => {
      this.onMediaSizeSettingChange_();
    });
    this.onMediaSizeSettingChange_();
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('capability')) {
      this.onMediaSizeSettingChange_();
    }
  }

  private onMediaSizeSettingChange_() {
    if (!this.capability) {
      return;
    }
    const valueToSet = JSON.stringify(this.getSettingValue('mediaSize'));
    for (const option of this.capability.option) {
      if (JSON.stringify(option) === valueToSet) {
        this.shadowRoot.querySelector('print-preview-settings-select')!
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
      this.setSetting('mediaSize', defaultOption, /*noSticky=*/ true);
    }
  }
}

export type MediaSizeSettingsElement = PrintPreviewMediaSizeSettingsElement;

declare global {
  interface HTMLElementTagNameMap {
    'print-preview-media-size-settings': PrintPreviewMediaSizeSettingsElement;
  }
}

customElements.define(
    PrintPreviewMediaSizeSettingsElement.is,
    PrintPreviewMediaSizeSettingsElement);
