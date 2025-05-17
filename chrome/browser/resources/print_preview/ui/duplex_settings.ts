// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';
import './icons.html.js';
import './settings_section.js';

import type {CrCheckboxElement} from 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import {IconsetMap} from 'chrome://resources/cr_elements/cr_icon/iconset_map.js';
import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {DuplexMode} from '../data/model.js';
import {getSelectDropdownBackground} from '../print_preview_utils.js';

import {getCss} from './duplex_settings.css.js';
import {getHtml} from './duplex_settings.html.js';
import {SelectMixin} from './select_mixin.js';
import {SettingsMixin} from './settings_mixin.js';

export interface PrintPreviewDuplexSettingsElement {
  $: {
    duplex: CrCheckboxElement,
  };
}

const PrintPreviewDuplexSettingsElementBase =
    SettingsMixin(SelectMixin(CrLitElement));

export class PrintPreviewDuplexSettingsElement extends
    PrintPreviewDuplexSettingsElementBase {
  static get is() {
    return 'print-preview-duplex-settings';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      dark: {type: Boolean},
      disabled: {type: Boolean},
      duplexManaged_: {type: Boolean},
      duplexShortEdgeManaged_: {type: Boolean},
      collapseOpened_: {type: Boolean},
      backgroundImages_: {type: String},
    };
  }

  accessor dark: boolean = false;
  accessor disabled: boolean = false;
  protected accessor duplexManaged_: boolean = false;
  protected accessor duplexShortEdgeManaged_: boolean = false;
  protected accessor collapseOpened_: boolean = false;

  // An inline svg corresponding to |icon| and the image for the dropdown arrow.
  protected accessor backgroundImages_: string = '';

  override connectedCallback() {
    super.connectedCallback();

    this.addSettingObserver('duplex.*', this.onDuplexSettingChange_.bind(this));
    this.onDuplexSettingChange_();

    this.addSettingObserver(
        'duplexShortEdge.*', this.onDuplexTypeChange_.bind(this));
    this.onDuplexTypeChange_();
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('dark')) {
      this.updateBackgroundImages_();
    }
  }

  private onDuplexSettingChange_() {
    this.$.duplex.checked = this.getSettingValue('duplex');
    this.duplexManaged_ = this.getSetting('duplex').setByGlobalPolicy;
    this.updateCollapseOpened_();
  }

  private onDuplexTypeChange_() {
    this.selectedValue = this.getSettingValue('duplexShortEdge') ?
        DuplexMode.SHORT_EDGE.toString() :
        DuplexMode.LONG_EDGE.toString();
    this.duplexShortEdgeManaged_ =
        this.getSetting('duplexShortEdge').setByGlobalPolicy;
    this.updateCollapseOpened_();
    this.updateBackgroundImages_();
  }

  protected onCheckboxChange_() {
    this.setSetting('duplex', this.$.duplex.checked);
  }

  override onProcessSelectChange(value: string) {
    this.setSetting(
        'duplexShortEdge', value === DuplexMode.SHORT_EDGE.toString());
  }

  protected updateCollapseOpened_() {
    this.collapseOpened_ = this.getSetting('duplexShortEdge').available &&
        (this.getSettingValue('duplex') as boolean);
  }

  /**
   * @param managed Whether the setting is managed by policy.
   * @return Whether the controls should be disabled.
   */
  protected getDisabled_(managed: boolean): boolean {
    return managed || this.disabled;
  }

  protected updateBackgroundImages_() {
    const icon =
        this.getSettingValue('duplexShortEdge') ? 'short-edge' : 'long-edge';
    const iconset = IconsetMap.getInstance().get('print-preview');
    assert(iconset);
    this.backgroundImages_ = getSelectDropdownBackground(iconset, icon, this);
  }
}

export type DuplexSettingsElement = PrintPreviewDuplexSettingsElement;

declare global {
  interface HTMLElementTagNameMap {
    'print-preview-duplex-settings': PrintPreviewDuplexSettingsElement;
  }
}

customElements.define(
    PrintPreviewDuplexSettingsElement.is, PrintPreviewDuplexSettingsElement);
