// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'os-settings-reset-page' is the page for Reset (powerwash) settings.
 */

import '../os_settings_page/os_settings_animated_pages.js';
import './reset_settings_card.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Section} from '../mojom-webui/routes.mojom-webui.js';

import {getTemplate} from './os_reset_page.html.js';

export class OsSettingsResetPageElement extends PolymerElement {
  static get is() {
    return 'os-settings-reset-page' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      section_: {
        type: Number,
        value: Section.kReset,
        readOnly: true,
      },
    };
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [OsSettingsResetPageElement.is]: OsSettingsResetPageElement;
  }
}

customElements.define(
    OsSettingsResetPageElement.is, OsSettingsResetPageElement);
