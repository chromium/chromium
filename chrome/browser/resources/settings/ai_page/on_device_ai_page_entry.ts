// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';
import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/icons.html.js';
import '../icons.html.js';

import type {CrCollapseElement} from 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';
import type {CrExpandButtonElement} from 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {OnDeviceAiCapability} from './on_device_ai_browser_proxy.js';
import {OnDeviceModelStatus} from './on_device_ai_browser_proxy.js';
import {getCss} from './on_device_ai_page_entry.css.js';
import {getHtml} from './on_device_ai_page_entry.html.js';

export interface SettingsOnDeviceAiPageEntryElement {
  $: {
    collapse: CrCollapseElement,
    delete: HTMLElement,
    expand: CrExpandButtonElement,
    modelDescription: HTMLElement,
    modelDetailsList: HTMLElement,
    modelMetadata: HTMLElement,
    modelTitle: HTMLElement,
  };
}

const SettingsOnDeviceAiPageEntryElementBase = I18nMixinLit(CrLitElement);

export class SettingsOnDeviceAiPageEntryElement extends
    SettingsOnDeviceAiPageEntryElementBase {
  static get is() {
    return 'settings-on-device-ai-page-entry';
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
      expanded_: {type: Boolean},
    };
  }

  accessor capability: OnDeviceAiCapability = {
    id: 0,
    status: OnDeviceModelStatus.READY,
    categoryTitle: '',
    description: '',
    modelName: '',
    size: '',
    details: [],
  };
  protected accessor expanded_: boolean = false;

  // Lets settings-focus-list-container move focus to this entry when the
  // focused entry is deleted. `delegatesFocus` is not used, because it would
  // also move focus to the delete button whenever the row is clicked.
  override focus() {
    this.$.expand.focus();
  }

  protected onExpandedChanged_(e: CustomEvent<{value: boolean}>) {
    this.expanded_ = e.detail.value;
  }

  protected onDeleteClick_() {
    this.fire('delete-click', this.capability.id);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-on-device-ai-page-entry': SettingsOnDeviceAiPageEntryElement;
  }
}

customElements.define(
    SettingsOnDeviceAiPageEntryElement.is, SettingsOnDeviceAiPageEntryElement);
