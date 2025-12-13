// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import '/shared/settings/prefs/prefs.js';
import '../icons.html.js';
import '../settings_page/settings_subpage.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import type {CrLinkRowElement} from 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import type {MetricsBrowserProxy} from '../metrics_browser_proxy.js';
import {MetricsBrowserProxyImpl} from '../metrics_browser_proxy.js';
import {routes} from '../route.js';
import {Router} from '../router.js';
import {SettingsViewMixin} from '../settings_page/settings_view_mixin.js';

import {getTemplate} from './privacy_sandbox_page.html.js';

export interface SettingsPrivacySandboxPageElement {
  $: {
    privacySandboxAdMeasurementLinkRow: CrLinkRowElement,
  };
}

const SettingsPrivacySandboxPageElementBase =
    SettingsViewMixin(I18nMixin(PrefsMixin(PolymerElement)));

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
      isPrivacySandboxRestricted_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('isPrivacySandboxRestricted'),
      },

      measurementLinkRowClass_: {
        type: String,
        value: () =>
            loadTimeData.getBoolean('isPrivacySandboxRestricted') ? '' : 'hr',
      },

    };
  }

  declare private isPrivacySandboxRestricted_: boolean;
  declare private measurementLinkRowClass_: string;
  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();

  private computePrivacySandboxTopicsSublabel_(): string {
    return this.i18n(
        this.isTopicsEnabled_() ? 'adPrivacyPageTopicsLinkRowSubLabelEnabled' :
                                  'adPrivacyPageTopicsLinkRowSubLabelDisabled');
  }

  private computePrivacySandboxFledgeSublabel_(): string {
    return this.i18n(
        this.isFledgeEnabled_() ? 'adPrivacyPageFledgeLinkRowSubLabelEnabled' :
                                  'adPrivacyPageFledgeLinkRowSubLabelDisabled');
  }

  private computePrivacySandboxAdMeasurementSublabel_(): string {
    return this.i18n(
        this.isAdMeasurementEnabled_() ?
            'adPrivacyPageAdMeasurementLinkRowSubLabelEnabled' :
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

  private isTopicsEnabled_() {
    return this.getPref('privacy_sandbox.m1.topics_enabled').value;
  }

  private isFledgeEnabled_() {
    return this.getPref('privacy_sandbox.m1.fledge_enabled').value;
  }

  private isAdMeasurementEnabled_() {
    return this.getPref('privacy_sandbox.m1.ad_measurement_enabled').value;
  }

  // SettingsViewMixin implementation.
  override getFocusConfig() {
    const map = new Map();

    if (routes.PRIVACY_SANDBOX_TOPICS) {
      map.set(
          routes.PRIVACY_SANDBOX_TOPICS.path, '#privacySandboxTopicsLinkRow');
    }

    if (routes.PRIVACY_SANDBOX_FLEDGE) {
      map.set(
          routes.PRIVACY_SANDBOX_FLEDGE.path, '#privacySandboxFledgeLinkRow');
    }

    if (routes.PRIVACY_SANDBOX_AD_MEASUREMENT) {
      map.set(
          routes.PRIVACY_SANDBOX_AD_MEASUREMENT.path,
          '#privacySandboxAdMeasurementLinkRow');
    }

    return map;
  }

  // SettingsViewMixin implementation.
  override focusBackButton() {
    this.shadowRoot!.querySelector('settings-subpage')!.focusBackButton();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-privacy-sandbox-page': SettingsPrivacySandboxPageElement;
  }
}

customElements.define(
    SettingsPrivacySandboxPageElement.is, SettingsPrivacySandboxPageElement);
