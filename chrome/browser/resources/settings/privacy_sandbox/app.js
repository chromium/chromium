// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import '../settings.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

// Those resources are loaded through settings.js as the privacy sandbox page
// lives outside regular settings, hence can't access those resources directly
// with |optimize_webui="true"|.
import {loadTimeData, MetricsBrowserProxy, MetricsBrowserProxyImpl, OpenWindowProxyImpl} from '../settings.js';

Polymer({
  is: 'privacy-sandbox-app',

  _template: html`{__html_template__}`,

  properties: {
    /**
     * Preferences state.
     */
    prefs: Object,
  },

  /** @private {?MetricsBrowserProxy} */
  metricsBrowserProxy_: null,

  /** @override */
  ready() {
    this.metricsBrowserProxy_ = MetricsBrowserProxyImpl.getInstance();
  },

  /** @private */
  onLearnMoreButtonClick_: function() {
    this.metricsBrowserProxy_.recordAction(
        'Settings.PrivacySandbox.OpenExplainer');
    OpenWindowProxyImpl.getInstance().openURL(
        loadTimeData.getString('privacySandboxURL'));
  },

  /**
   * @param {!Event} event
   * @private
   */
  onApiToggleButtonChange_(event) {
    const privacySandboxApisEnabled = event.target.checked;
    this.metricsBrowserProxy_.recordAction(
        privacySandboxApisEnabled ? 'Settings.PrivacySandbox.ApisEnabled' :
                                    'Settings.PrivacySandbox.ApisDisabled');
  },
});
