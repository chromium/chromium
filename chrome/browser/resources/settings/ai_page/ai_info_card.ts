// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-ai-info-card' is the top info card in AI settings page.
 */
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import '../settings_page/settings_section.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {loadTimeData} from '../i18n_setup.js';

import {getCss} from './ai_info_card.css.js';
import {getHtml} from './ai_info_card.html.js';

export class SettingsAiInfoCardElement extends CrLitElement {
  static get is() {
    return 'settings-ai-info-card';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  protected isManaged_(): boolean {
    return loadTimeData.getBoolean('isManaged');
  }

  protected getIcon3_(): string {
    return this.isManaged_() ? loadTimeData.getString('managedByIcon') :
                               'settings20:account-box';
  }
}

export type AiInfoCardElement = SettingsAiInfoCardElement;

declare global {
  interface HTMLElementTagNameMap {
    'settings-ai-info-card': SettingsAiInfoCardElement;
  }
}

customElements.define(SettingsAiInfoCardElement.is, SettingsAiInfoCardElement);
