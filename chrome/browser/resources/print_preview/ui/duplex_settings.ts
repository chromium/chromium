// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';
import 'chrome://resources/cr_elements/md_select.css.js';
import './icons.html.js';
import './print_preview_shared.css.js';
import './settings_section.js';

import type {CrCheckboxElement} from 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import {IconsetMap} from 'chrome://resources/cr_elements/cr_icon/iconset_map.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DuplexMode} from '../data/model.js';
import {getSelectDropdownBackground} from '../print_preview_utils.js';

import {getTemplate} from './duplex_settings.html.js';
import {SelectMixin} from './select_mixin.js';
import {SettingsMixin} from './settings_mixin.js';

export interface PrintPreviewDuplexSettingsElement {
  $: {
    duplex: CrCheckboxElement,
  };
}

const PrintPreviewDuplexSettingsElementBase =
    SettingsMixin(SelectMixin(PolymerElement));

export class PrintPreviewDuplexSettingsElement extends
    PrintPreviewDuplexSettingsElementBase {
  static get is() {
    return 'print-preview-duplex-settings';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      dark: Boolean,

      disabled: Boolean,

      /**
       * Mirroring the enum so that it can be used from HTML bindings.
       */
      duplexValueEnum_: {
        type: Object,
        value: DuplexMode,
      },
    };
  }

  static get observers() {
    return [
      'onDuplexSettingChange_(settings.duplex.*)',
      'onDuplexTypeChange_(settings.duplexShortEdge.*)',
    ];
  }

  dark: boolean;
  disabled: boolean;

  private onDuplexSettingChange_() {
    this.$.duplex.checked = this.getSettingValue('duplex');
  }

  private onDuplexTypeChange_() {
    this.selectedValue = this.getSettingValue('duplexShortEdge') ?
        DuplexMode.SHORT_EDGE.toString() :
        DuplexMode.LONG_EDGE.toString();
  }

  private onCheckboxChange_() {
    this.setSetting('duplex', this.$.duplex.checked);
  }

  override onProcessSelectChange(value: string) {
    this.setSetting(
        'duplexShortEdge', value === DuplexMode.SHORT_EDGE.toString());
  }

  /**
   * @return Whether to expand the collapse for the dropdown.
   */
  private getOpenCollapse_(): boolean {
    return this.getSetting('duplexShortEdge').available &&
        (this.getSettingValue('duplex') as boolean);
  }

  /**
   * @param managed Whether the setting is managed by policy.
   * @param disabled value of this.disabled
   * @return Whether the controls should be disabled.
   */
  private getDisabled_(managed: boolean, disabled: boolean): boolean {
    return managed || disabled;
  }

  /**
   * @return An inline svg corresponding to |icon| and the image for
   *     the dropdown arrow.
   */
  private getBackgroundImages_(): string {
    const icon =
        this.getSettingValue('duplexShortEdge') ? 'short-edge' : 'long-edge';
    const iconset = IconsetMap.getInstance().get('print-preview');
    assert(iconset);
    return getSelectDropdownBackground(iconset, icon, this);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'print-preview-duplex-settings': PrintPreviewDuplexSettingsElement;
  }
}

customElements.define(
    PrintPreviewDuplexSettingsElement.is, PrintPreviewDuplexSettingsElement);
