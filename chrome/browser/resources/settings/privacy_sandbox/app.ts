// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://resources/cr_elements/cr_page_host_style_css.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import './icons.js';
import '../lazy_load.js';
import '../settings.js';

import {addWebUIListener} from 'chrome://resources/js/cr.m.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

// Those resources are loaded through lazy_load.js and settings.js as the
// privacy sandbox page lives outside regular settings, hence can't access those
// resources directly with |optimize_webui="true"|.
import {CrDialogElement} from '../lazy_load.js';
import {CrSettingsPrefs, HatsBrowserProxyImpl, loadTimeData, MetricsBrowserProxy, MetricsBrowserProxyImpl, PrefsMixin, SettingsToggleButtonElement, TrustSafetyInteraction} from '../settings.js';

import {getTemplate} from './app.html.js';
import {FlocIdentifier, PrivacySandboxBrowserProxy, PrivacySandboxBrowserProxyImpl} from './privacy_sandbox_browser_proxy.js';

/** Views of the PrivacySandboxSettings page. */
export enum PrivacySandboxSettingsView {
  MAIN = 'main',
  LEARN_MORE_DIALOG = 'learnMoreDialog',
  AD_PERSONALIZATION_DIALOG = 'adPersonalizationDialog',
  AD_MEASUREMENT_DIALOG = 'adMeasurementDialog',
  SPAM_AND_FRAUD_DIALOG = 'spamAndFraudDialog',
}

const PrivacySandboxAppElementBase = PrefsMixin(PolymerElement);

export class PrivacySandboxAppElement extends PrivacySandboxAppElementBase {
  static get is() {
    return 'privacy-sandbox-app';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      flocId_: Object,

      privacySandboxSettings3Enabled_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('privacySandboxSettings3Enabled'),
      },

      /**
       * Valid privacy sandbox settings view states.
       */
      privacySandboxSettingsViewEnum_: {
        type: Object,
        value: PrivacySandboxSettingsView,
      },

      /** The current view. */
      privacySandboxSettingsView_: {
        type: String,
        value: PrivacySandboxSettingsView.MAIN,
      },
    };
  }

  static get observers() {
    return ['onFlocChanged_(prefs.generated.floc_enabled.*)'];
  }

  private flocId_: FlocIdentifier;
  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();
  private privacySandboxBrowserProxy_: PrivacySandboxBrowserProxy =
      PrivacySandboxBrowserProxyImpl.getInstance();
  private privacySandboxSettings3Enabled_: boolean;
  privacySandboxSettingsView_: PrivacySandboxSettingsView;

  ready() {
    super.ready();

    chrome.metricsPrivate.recordSparseHashable(
        'WebUI.Settings.PathVisited', '/privacySandbox');

    this.privacySandboxBrowserProxy_.getFlocId().then(id => this.flocId_ = id);
    addWebUIListener(
        'floc-id-changed', (id: FlocIdentifier) => this.flocId_ = id);

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

    HatsBrowserProxyImpl.getInstance().trustSafetyInteractionOccurred(
        TrustSafetyInteraction.OPENED_PRIVACY_SANDBOX);
  }

  private onFlocChanged_() {
    this.privacySandboxBrowserProxy_.getFlocId().then(id => this.flocId_ = id);
  }

  private onResetFlocClick_() {
    this.privacySandboxBrowserProxy_.resetFlocId();
  }

  private onApiToggleButtonChange_(event: Event) {
    const privacySandboxApisEnabled =
        (event.target as SettingsToggleButtonElement).checked;
    this.metricsBrowserProxy_.recordAction(
        privacySandboxApisEnabled ? 'Settings.PrivacySandbox.ApisEnabled' :
                                    'Settings.PrivacySandbox.ApisDisabled');
    this.setPrefValue('privacy_sandbox.manually_controlled', true);
  }

  private onFlocToggleButtonChange_(event: Event) {
    const flocEnabled = (event.target as SettingsToggleButtonElement).checked;
    this.metricsBrowserProxy_.recordAction(
        flocEnabled ? 'Settings.PrivacySandbox.FlocEnabled' :
                      'Settings.PrivacySandbox.FlocDisabled');
  }

  private showFragment_(view: PrivacySandboxSettingsView): boolean {
    return this.privacySandboxSettingsView_ === view;
  }

  private onDialogClose_() {
    this.privacySandboxSettingsView_ = PrivacySandboxSettingsView.MAIN;
  }

  private onLearnMoreClick_(e: Event) {
    // Stop the propagation of events, so that clicking on links inside
    // actionable items won't trigger action.
    e.stopPropagation();
    this.privacySandboxSettingsView_ =
        PrivacySandboxSettingsView.LEARN_MORE_DIALOG;
  }

  private onAdPersonalizationRowClick_() {
    this.privacySandboxSettingsView_ =
        PrivacySandboxSettingsView.AD_PERSONALIZATION_DIALOG;
  }

  private onAdMeasurementRowClick_() {
    this.privacySandboxSettingsView_ =
        PrivacySandboxSettingsView.AD_MEASUREMENT_DIALOG;
  }

  private onSpamAndFraudRowClick_() {
    this.privacySandboxSettingsView_ =
        PrivacySandboxSettingsView.SPAM_AND_FRAUD_DIALOG;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'privacy-sandbox-app': PrivacySandboxAppElement;
  }
}

customElements.define(PrivacySandboxAppElement.is, PrivacySandboxAppElement);
