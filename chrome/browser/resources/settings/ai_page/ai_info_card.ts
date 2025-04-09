// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-ai-info-card' is the top info card in AI settings page.
 */
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import '../settings_shared.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';

import {getTemplate} from './ai_info_card.html.js';

export class SettingsAiInfoCardElement extends PolymerElement {
  static get is() {
    return 'settings-ai-info-card';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      icon3_: {
        type: String,
        computed: 'computeIcon3_()',
      },
    };
  }

  declare private icon3_: string;

  private isManaged_(): boolean {
    return loadTimeData.getBoolean('isManaged');
  }

  private computeIcon3_(): string {
    return this.isManaged_() ? loadTimeData.getString('managedByIcon') :
                               'settings20:account-box';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-ai-info-card': SettingsAiInfoCardElement;
  }
}

customElements.define(SettingsAiInfoCardElement.is, SettingsAiInfoCardElement);
