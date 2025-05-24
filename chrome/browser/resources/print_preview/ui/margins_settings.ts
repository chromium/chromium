// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './settings_section.js';

import {getCss as getMdSelectLitCss} from 'chrome://resources/cr_elements/md_select_lit.css.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {MarginsType} from '../data/margins.js';
import {State} from '../data/state.js';

import {getHtml} from './margins_settings.html.js';
import {getCss as getPrintPreviewSharedCss} from './print_preview_shared.css.js';
import {SelectMixin} from './select_mixin.js';
import {SettingsMixin} from './settings_mixin.js';


const PrintPreviewMarginsSettingsElementBase =
    SettingsMixin(SelectMixin(CrLitElement));

export class PrintPreviewMarginsSettingsElement extends
    PrintPreviewMarginsSettingsElementBase {
  static get is() {
    return 'print-preview-margins-settings';
  }

  static override get styles() {
    return [
      getPrintPreviewSharedCss(),
      getMdSelectLitCss(),
    ];
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      disabled: {type: Boolean},
      state: {type: Number},
      marginsDisabled_: {type: Boolean},
      pagesPerSheet_: {type: Number},
    };
  }

  accessor disabled: boolean = false;
  accessor state: State = State.NOT_READY;
  protected accessor marginsDisabled_: boolean = false;
  private accessor pagesPerSheet_: number = 1;
  private loaded_: boolean = false;

  override connectedCallback() {
    super.connectedCallback();

    this.addSettingObserver('pagesPerSheet.value', (newValue: number) => {
      this.pagesPerSheet_ = newValue;
    });
    this.pagesPerSheet_ = this.getSetting('pagesPerSheet').value;

    this.addSettingObserver('margins.value', (newValue: MarginsType) => {
      this.selectedValue = newValue.toString();
    });
    this.selectedValue = this.getSetting('margins').value.toString();

    this.addSettingObserver(
        'mediaSize.value', this.onMediaSizeOrLayoutChange_.bind(this));
    this.addSettingObserver(
        'layout.value', this.onMediaSizeOrLayoutChange_.bind(this));
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedProperties.has('state')) {
      if (this.state === State.READY) {
        this.loaded_ = true;
      }
    }

    if (changedProperties.has('disabled') ||
        changedPrivateProperties.has('pagesPerSheet_')) {
      this.marginsDisabled_ = this.pagesPerSheet_ > 1 || this.disabled;
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedPrivateProperties.has('pagesPerSheet_')) {
      if (this.pagesPerSheet_ > 1) {
        this.setSetting('margins', MarginsType.DEFAULT);
      }
    }
  }

  private onMediaSizeOrLayoutChange_() {
    if (this.loaded_ &&
        this.getSetting('margins').value === MarginsType.CUSTOM) {
      this.setSetting('margins', MarginsType.DEFAULT);
    }
  }

  override onProcessSelectChange(value: string) {
    this.setSetting('margins', parseInt(value, 10));
  }
}

export type MarginsSettingsElement = PrintPreviewMarginsSettingsElement;

declare global {
  interface HTMLElementTagNameMap {
    'print-preview-margins-settings': PrintPreviewMarginsSettingsElement;
  }
}

customElements.define(
    PrintPreviewMarginsSettingsElement.is, PrintPreviewMarginsSettingsElement);
