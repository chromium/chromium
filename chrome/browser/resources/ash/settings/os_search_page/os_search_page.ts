// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'os-settings-search-page' contains search and assistant settings.
 */
import '../settings_shared.css.js';
import '../os_settings_page/os_settings_animated_pages.js';
import '../os_settings_page/os_settings_subpage.js';
import './search_and_assistant_settings_card.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {isAssistantAllowed, isQuickAnswersSupported} from '../common/load_time_booleans.js';
import {PrefsState} from '../common/types.js';
import {Section} from '../mojom-webui/routes.mojom-webui.js';

import {getTemplate} from './os_search_page.html.js';

export class OsSettingsSearchPageElement extends PolymerElement {
  static get is() {
    return 'os-settings-search-page' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      prefs: {
        type: Object,
        notify: true,
      },

      section_: {
        type: Number,
        value: Section.kSearchAndAssistant,
        readOnly: true,
      },

      isQuickAnswersSupported_: {
        type: Boolean,
        value: () => {
          return isQuickAnswersSupported();
        },
      },

      /** Can be disallowed due to flag, policy, locale, etc. */
      isAssistantAllowed_: {
        type: Boolean,
        value: () => {
          return isAssistantAllowed();
        },
      },
    };
  }

  prefs: PrefsState;
  private isAssistantAllowed_: boolean;
  private section_: Section;
  private isQuickAnswersSupported_: boolean;
}

declare global {
  interface HTMLElementTagNameMap {
    [OsSettingsSearchPageElement.is]: OsSettingsSearchPageElement;
  }
}

customElements.define(
    OsSettingsSearchPageElement.is, OsSettingsSearchPageElement);
