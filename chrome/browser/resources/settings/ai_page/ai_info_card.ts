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

import {getTemplate} from './ai_info_card.html.js';

export class SettingsAiInfoCardElement extends PolymerElement {
  static get is() {
    return 'settings-ai-info-card';
  }

  static get template() {
    return getTemplate();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-ai-info-card': SettingsAiInfoCardElement;
  }
}

customElements.define(SettingsAiInfoCardElement.is, SettingsAiInfoCardElement);
