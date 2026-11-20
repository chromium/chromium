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
import type {CrIconElement} from 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {assertNotReached, assertNotReachedCase} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {OnDeviceAiCapability} from './on_device_ai_browser_proxy.js';
import {OnDeviceModelCategory, OnDeviceModelStatus} from './on_device_ai_browser_proxy.js';
import {getCss} from './on_device_ai_page_entry.css.js';
import {getHtml} from './on_device_ai_page_entry.html.js';

// Leading icon shown for each model category.
const CATEGORY_ICONS: Record<OnDeviceModelCategory, string> = {
  [OnDeviceModelCategory.GENERAL_PURPOSE]: 'settings20:pen-spark',
  [OnDeviceModelCategory.PROOFREADING]: 'settings20:spellcheck-spark',
  [OnDeviceModelCategory.SUMMARIZING]: 'settings20:summarize-auto-2',
  [OnDeviceModelCategory.LIVE_TRANSCRIPTION]: 'settings20:mic-detect-auto',
};

export interface SettingsOnDeviceAiPageEntryElement {
  $: {
    collapse: CrCollapseElement,
    expand: CrExpandButtonElement,
    modelDescription: HTMLElement,
    modelDetailsList: HTMLElement,
    modelIcon: CrIconElement,
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
    category: OnDeviceModelCategory.GENERAL_PURPOSE,
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

  protected getIcon_(): string {
    return CATEGORY_ICONS[this.capability.category];
  }

  protected onExpandedChanged_(e: CustomEvent<{value: boolean}>) {
    this.expanded_ = e.detail.value;
  }

  protected onDeleteClick_() {
    this.fire('delete-click', this.capability.id);
  }

  protected getMetadataText_(): string {
    switch (this.capability.status) {
      case OnDeviceModelStatus.READY:
        return this.i18n(
            'onDeviceAiModelMetadata', this.capability.modelName,
            this.capability.size);
      case OnDeviceModelStatus.DOWNLOADING:
        return this.i18n('onDeviceAiModelDownloading');
      case OnDeviceModelStatus.UPDATING:
        return this.i18n('onDeviceAiModelUpdating');
      case OnDeviceModelStatus.DOWNLOADABLE:
        assertNotReached();
      default:
        assertNotReachedCase(this.capability.status);
    }
  }

  protected getStatusIcon_(): string {
    switch (this.capability.status) {
      case OnDeviceModelStatus.DOWNLOADING:
        return 'settings20:downloading';
      case OnDeviceModelStatus.UPDATING:
        return 'settings20:update';
      case OnDeviceModelStatus.READY:
      case OnDeviceModelStatus.DOWNLOADABLE:
        assertNotReached();
      default:
        assertNotReachedCase(this.capability.status);
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-on-device-ai-page-entry': SettingsOnDeviceAiPageEntryElement;
  }
}

customElements.define(
    SettingsOnDeviceAiPageEntryElement.is, SettingsOnDeviceAiPageEntryElement);
