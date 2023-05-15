// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_components/settings_prefs/prefs.js';

import {PrefsMixin} from 'chrome://resources/cr_components/settings_prefs/prefs_mixin.js';
import {CrLinkRowElement} from 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../../i18n_setup.js';
import {MetricsBrowserProxy, MetricsBrowserProxyImpl} from '../../metrics_browser_proxy.js';
import {routes} from '../../route.js';
import {Router} from '../../router.js';

import {getTemplate} from './privacy_sandbox_page.html.js';

export interface SettingsPrivacySandboxPageElement {
  $: {
    privacySandboxAdMeasurementLinkRow: CrLinkRowElement,
  };
}

const SettingsPrivacySandboxPageElementBase =
    I18nMixin(PrefsMixin(PolymerElement));

export class SettingsPrivacySandboxPageElement extends
    SettingsPrivacySandboxPageElementBase {
  static get is() {
    return 'settings-privacy-sandbox-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Preferences state.
       */
      prefs: {
        type: Object,
        notify: true,
      },

      isPrivacySandboxRestricted_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('isPrivacySandboxRestricted'),
      },
    };
  }

  private isPrivacySandboxRestricted_: boolean;
  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();

  private computePrivacySandboxTopicsSublabel_(): string {
    const enabled = this.getPref('privacy_sandbox.m1.topics_enabled').value;
    return this.i18n(
        enabled ? 'adPrivacyPageTopicsLinkRowSubLabelEnabled' :
                  'adPrivacyPageTopicsLinkRowSubLabelDisabled');
  }

  private computePrivacySandboxFledgeSublabel_(): string {
    const enabled = this.getPref('privacy_sandbox.m1.fledge_enabled').value;
    return this.i18n(
        enabled ? 'adPrivacyPageFledgeLinkRowSubLabelEnabled' :
                  'adPrivacyPageFledgeLinkRowSubLabelDisabled');
  }

  private computePrivacySandboxAdMeasurementSublabel_(): string {
    const enabled =
        this.getPref('privacy_sandbox.m1.ad_measurement_enabled').value;
    return this.i18n(
        enabled ? 'adPrivacyPageAdMeasurementLinkRowSubLabelEnabled' :
                  'adPrivacyPageAdMeasurementLinkRowSubLabelDisabled');
  }

  private onPrivacySandboxTopicsClick_() {
    this.metricsBrowserProxy_.recordAction(
        'Settings.PrivacySandbox.Topics.Opened');
    Router.getInstance().navigateTo(routes.PRIVACY_SANDBOX_TOPICS);
  }

  private onPrivacySandboxFledgeClick_() {
    this.metricsBrowserProxy_.recordAction(
        'Settings.PrivacySandbox.Fledge.Opened');
    Router.getInstance().navigateTo(routes.PRIVACY_SANDBOX_FLEDGE);
  }

  private onPrivacySandboxAdMeasurementClick_() {
    this.metricsBrowserProxy_.recordAction(
        'Settings.PrivacySandbox.AdMeasurement.Opened');
    Router.getInstance().navigateTo(routes.PRIVACY_SANDBOX_AD_MEASUREMENT);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-privacy-sandbox-page': SettingsPrivacySandboxPageElement;
  }
}

customElements.define(
    SettingsPrivacySandboxPageElement.is, SettingsPrivacySandboxPageElement);
