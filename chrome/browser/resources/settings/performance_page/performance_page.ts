// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import '../controls/settings_toggle_button.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import {OpenWindowProxyImpl} from '../open_window_proxy.js';
import {PrefsMixin} from '../prefs/prefs_mixin.js';

import {PerformanceBrowserProxy, PerformanceBrowserProxyImpl} from './performance_browser_proxy.js';
import {getTemplate} from './performance_page.html.js';
// clang-format on

const SettingsPerformancePageElementBase = PrefsMixin(PolymerElement);

export class SettingsPerformancePageElement extends
    SettingsPerformancePageElementBase {
  static get is() {
    return 'settings-performance-page';
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

  private browserProxy_: PerformanceBrowserProxy =
      PerformanceBrowserProxyImpl.getInstance();

  private onLearnMoreOrSendFeedbackClick_(e: CustomEvent<string>) {
    switch (e.detail) {
      case 'highEfficiencyLearnMore':
        OpenWindowProxyImpl.getInstance().openURL(
            loadTimeData.getString('highEfficiencyLearnMoreUrl'));
        break;
      case 'highEfficiencySendFeedback':
        this.browserProxy_.openHighEfficiencyFeedbackDialog();
        break;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-performance-page': SettingsPerformancePageElement;
  }
}

customElements.define(
    SettingsPerformancePageElement.is, SettingsPerformancePageElement);
