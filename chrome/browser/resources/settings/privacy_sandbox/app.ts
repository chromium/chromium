// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://resources/cr_elements/cr_page_host_style_css.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import '../settings.js';

import {addWebUIListener} from 'chrome://resources/js/cr.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

// Those resources are loaded through settings.js as the privacy sandbox page
// lives outside regular settings, hence can't access those resources directly
// with |optimize_webui="true"|.
import {CrButtonElement, CrSettingsPrefs, HatsBrowserProxyImpl, loadTimeData, MetricsBrowserProxy, MetricsBrowserProxyImpl, PrefsMixin, SettingsToggleButtonElement, TrustSafetyInteraction} from '../settings.js';

import {FlocIdentifier, PrivacySandboxBrowserProxy, PrivacySandboxBrowserProxyImpl} from './privacy_sandbox_browser_proxy.js';

export interface PrivacySandboxAppElement {
  $: {
    flocId: HTMLElement,
    flocStatus: HTMLElement,
    flocToggleButton: SettingsToggleButtonElement,
    flocUpdatedOn: HTMLElement,
    resetFlocIdButton: CrButtonElement,
  };
}

const PrivacySandboxAppElementBase = PrefsMixin(PolymerElement);

export class PrivacySandboxAppElement extends PrivacySandboxAppElementBase {
  static get is() {
    return 'privacy-sandbox-app';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      flocId_: Object,
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
}

declare global {
  interface HTMLElementTagNameMap {
    'privacy-sandbox-app': PrivacySandboxAppElement;
  }
}

customElements.define(PrivacySandboxAppElement.is, PrivacySandboxAppElement);
