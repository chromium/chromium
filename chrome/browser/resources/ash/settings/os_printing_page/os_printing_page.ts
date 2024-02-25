// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../os_settings_page/os_settings_animated_pages.js';
import '../os_settings_page/os_settings_subpage.js';
import './cups_printers.js';
import '../settings_shared.css.js';
import './printing_settings_card.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Section} from '../mojom-webui/routes.mojom-webui.js';

import {getTemplate} from './os_printing_page.html.js';

export class OsSettingsPrintingPageElement extends PolymerElement {
  static get is() {
    return 'os-settings-printing-page' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Preferences state.
       * */
      prefs: {
        type: Object,
        notify: true,
      },

      section_: {
        type: Number,
        value: Section.kPrinting,
        readOnly: true,
      },

      /**
       * Printer search string.
       * */
      searchTerm: {
        type: String,
      },
    };
  }

  prefs: object;
  searchTerm: string;

  private section_: Section;
}

declare global {
  interface HTMLElementTagNameMap {
    [OsSettingsPrintingPageElement.is]: OsSettingsPrintingPageElement;
  }
}

customElements.define(
    OsSettingsPrintingPageElement.is, OsSettingsPrintingPageElement);
