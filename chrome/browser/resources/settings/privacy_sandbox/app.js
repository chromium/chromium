// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://resources/cr_elements/cr_page_host_style_css.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import '../settings.js';

import {addWebUIListener} from 'chrome://resources/js/cr.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

// Those resources are loaded through settings.js as the privacy sandbox page
// lives outside regular settings, hence can't access those resources directly
// with |optimize_webui="true"|.
import {CrSettingsPrefs, HatsBrowserProxyImpl, loadTimeData, MetricsBrowserProxy, MetricsBrowserProxyImpl, OpenWindowProxyImpl, PrefsBehavior} from '../settings.js';

import {FlocIdentifier, PrivacySandboxBrowserProxy, PrivacySandboxBrowserProxyImpl} from './privacy_sandbox_browser_proxy.js';

Polymer({
  is: 'privacy-sandbox-app',

  _template: html`{__html_template__}`,

  behaviors: [
    PrefsBehavior,
  ],

  properties: {
    /** @private */
    privacySandboxSettings2Enabled_: {
      type: Boolean,
      value: () => loadTimeData.getBoolean('privacySandboxSettings2Enabled'),
    },

    /** @private {!FlocIdentifier} */
    flocId_: {
      type: Object,
    },
  },

  observers: ['onFlocChanged_(prefs.generated.floc_enabled.*)'],

  /** @private {?MetricsBrowserProxy} */
  metricsBrowserProxy_: null,

  /** @private {?PrivacySandboxBrowserProxy} */
  privacySandboxBrowserProxy_: null,

  /** @override */
  ready() {
    this.metricsBrowserProxy_ = MetricsBrowserProxyImpl.getInstance();
    chrome.metricsPrivate.recordSparseHashable(
        'WebUI.Settings.PathVisited', '/privacySandbox');

    this.privacySandboxBrowserProxy_ =
        PrivacySandboxBrowserProxyImpl.getInstance();
    this.privacySandboxBrowserProxy_.getFlocId().then(id => this.flocId_ = id);
    addWebUIListener('floc-id-changed', id => this.flocId_ = id);

    // Make the required policy strings available at the window level. This is
    // expected by cr-elements related to policy.
    window.CrPolicyStrings = {
      controlledSettingExtension:
          loadTimeData.getString('controlledSettingExtension'),
      controlledSettingExtensionWithoutName:
          loadTimeData.getString('controlledSettingExtensionWithoutName'),
      controlledSettingPolicy:
          loadTimeData.getString('controlledSettingPolicy'),
      controlledSettingRecommendedMatches:
          loadTimeData.getString('controlledSettingRecommendedMatches'),
      controlledSettingRecommendedDiffers:
          loadTimeData.getString('controlledSettingRecommendedDiffers'),
    };

    CrSettingsPrefs.initialized.then(() => {
      // Wait for preferences to be initialized before writing.
      this.setPrefValue('privacy_sandbox.page_viewed', true);
    });

    HatsBrowserProxyImpl.getInstance().tryShowPrivacySandboxSurvey();
  },

  /** @private */
  apiToggleButtonClass_: function() {
    return this.privacySandboxSettings2Enabled_ ? 'hr updated-toggle-button' :
                                                  'hr';
  },

  /** @private */
  onFlocChanged_: function() {
    this.privacySandboxBrowserProxy_.getFlocId().then(id => this.flocId_ = id);
  },

  /** @private */
  onLearnMoreButtonClick_: function() {
    this.metricsBrowserProxy_.recordAction(
        'Settings.PrivacySandbox.OpenExplainer');
    OpenWindowProxyImpl.getInstance().openURL(
        loadTimeData.getString('privacySandboxURL'));
  },

  /** @private */
  onResetFlocClick_: function() {
    this.privacySandboxBrowserProxy_.resetFlocId();
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
    this.setPrefValue('privacy_sandbox.manually_controlled', true);
  },

  /**
   * @param {!Event} event
   * @private
   */
  onFlocToggleButtonChange_(event) {
    const flocEnabled = event.target.checked;
    this.metricsBrowserProxy_.recordAction(
        flocEnabled ? 'Settings.PrivacySandbox.FlocEnabled' :
                      'Settings.PrivacySandbox.FlocDisabled');
  },
});
