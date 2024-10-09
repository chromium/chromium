// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import '../controls/settings_toggle_button.js';
import '../settings_columned_section.css.js';
import '../settings_shared.css.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {FeatureOptInState} from './constants.js';
import {getTemplate} from './history_search_page.html.js';

const SettingsHistorySearchPageElementBase = PrefsMixin(PolymerElement);

export class SettingsHistorySearchPageElement extends
    SettingsHistorySearchPageElementBase {
  static get is() {
    return 'settings-history-search-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      enableAiSettingsPageRefresh_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('enableAiSettingsPageRefresh'),
      },

      featureOptInStateEnum_: {
        type: Object,
        value: FeatureOptInState,
      },

      numericUncheckedValues_: {
        type: Array,
        value: () =>
            [FeatureOptInState.DISABLED, FeatureOptInState.NOT_INITIALIZED],
      },
    };
  }

  private enableAiSettingsPageRefresh_: boolean;
  private numericUncheckedValues_: FeatureOptInState[];

  private onHistorySearchLinkoutClick_() {
    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('historySearchDataHomeUrl'));
  }

  private onLearnMoreClick_(event: Event) {
    // Stop the propagation of events, so that clicking on the 'Learn More' link
    // won't trigger the external linkout action on the parent cr-link-row
    // element.
    event.stopPropagation();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-history-search-page': SettingsHistorySearchPageElement;
  }
}

customElements.define(
    SettingsHistorySearchPageElement.is, SettingsHistorySearchPageElement);
