// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import '../settings_columned_section.css.js';
import '../settings_page/settings_subpage.js';
import '../settings_shared.css.js';
import '../controls/settings_toggle_button.js';

import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './ai_mode_search_page.html.js';

export class SettingsAiModeSearchPageElement extends PolymerElement {
  static get is() {
    return 'settings-ai-mode-search-page';
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
    };
  }

  declare prefs: {[key: string]: any};

  private onLearnMoreRowClick_() {
    OpenWindowProxyImpl.getInstance().openUrl(
        'https://support.google.com/chrome?p=ai_mode_search');
  }

  private onLearnMoreClick_(event: Event) {
    event.stopPropagation();
  }

  private onGoogleSearchHistoryClick_() {
    window.open('https://myactivity.google.com/product/search');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-ai-mode-search-page': SettingsAiModeSearchPageElement;
  }
}

customElements.define(
    SettingsAiModeSearchPageElement.is, SettingsAiModeSearchPageElement);
