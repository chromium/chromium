// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './content_setting_pattern_source.js';
import './pref_display.js';
import './mojo_timedelta.js';
import './cr_frame_list.js';
import 'chrome://resources/cr_elements/cr_tab_box/cr_tab_box.js';
import './privacy_sandbox_internals.mojom-webui.js';

import {CustomElement} from 'chrome://resources/js/custom_element.js';

import {ContentSettingsType} from './content_settings_types.mojom-webui.js';
import type {CrFrameListElement} from './cr_frame_list.js';
import {getTemplate} from './internals_page.html.js';
import {PrivacySandboxInternalsBrowserProxy} from './privacy_sandbox_internals_browser_proxy.js';
import {Router} from './router.js';
import type {RouteObserver} from './router.js';
import type {LogicalFn} from './value_display.js';
import {defaultLogicalFn, timestampLogicalFn} from './value_display.js';

interface PrefConfig {
  logicalFn?: LogicalFn;
}

const tpcdExperimentPrefs: Map<string, PrefConfig> = new Map(Object.entries({
  'tpcd_experiment.client_state': {},
  'tpcd_experiment.client_state_version': {},
  'tpcd_experiment.profile_state': {},
  // Not directly related, but relevant.
  'uninstall_metrics.installation_date2': {},
  'profile.cookie_controls_mode': {},
  'profile.cookie_block_truncated': {},
}));

const trackingProtectionPrefNames: Map<
    string, PrefConfig> = new Map(Object.entries({
  'profile.managed_cookies_allowed_for_urls': {},
  'enable_do_not_track': {},
  'tracking_protection.fingerprinting_protection_enabled': {},
  'tracking_protection.ip_protection_enabled': {},
  'tracking_protection.ip_protection_initialized_by_dogfood': {},
  'tracking_protection.reminder_status': {},
  'tracking_protection.survey_window_start_time':
      {logicalFn: timestampLogicalFn},
  'tracking_protection.tracking_protection_onboarding_status': {},
  'tracking_protection.tracking_protection_eligible_since':
      {logicalFn: timestampLogicalFn},
  'tracking_protection.tracking_protection_onboarded_since':
      {logicalFn: timestampLogicalFn},
  'tracking_protection.tracking_protection_notice_last_shown':
      {logicalFn: timestampLogicalFn},
  'tracking_protection.tracking_protection_onboarding_acked_since':
      {logicalFn: timestampLogicalFn},
  'tracking_protection.tracking_protection_onboarding_acked': {},
  'tracking_protection.tracking_protection_onboarding_ack_action': {},
  'tracking_protection.tracking_protection_onboarding_notice_first_requested':
      {logicalFn: timestampLogicalFn},
  'tracking_protection.tracking_protection_onboarding_notice_last_requested':
      {logicalFn: timestampLogicalFn},
  'tracking_protection.tracking_protection_offboarded': {},
  'tracking_protection.tracking_protection_offboarded_since':
      {logicalFn: timestampLogicalFn},
  'tracking_protection.tracking_protection_offboarding_ack_action': {},
  'tracking_protection.block_all_3pc_toggle_enabled': {},
  'tracking_protection.tracking_protection_level': {},
  'tracking_protection.tracking_protection_3pcd_enabled': {},
  'tracking_protection.tracking_protection_sentiment_survey_group': {},
  'tracking_protection.tracking_protection_sentiment_survey_start_time':
      {logicalFn: timestampLogicalFn},
  'tracking_protection.tracking_protection_sentiment_survey_end_time':
      {logicalFn: timestampLogicalFn},
  'tracking_protection.tracking_protection_silent_onboarding_status': {},
  'tracking_protection.tracking_protection_silent_eligible_since':
      {logicalFn: timestampLogicalFn},
  'tracking_protection.tracking_protection_silent_onboarded_since':
      {logicalFn: timestampLogicalFn},
}));

const advertisingPrefNames: Map<string, PrefConfig> = new Map(Object.entries({
  'privacy_sandbox.m1.consent_decision_made': {},
  'privacy_sandbox.m1.eea_notice_acknowledged': {},
  'privacy_sandbox.m1.row_notice_acknowledged': {},
  'privacy_sandbox.m1.restricted_notice_acknowledged': {},
  'privacy_sandbox.m1.prompt_suppressed': {},
  'privacy_sandbox.m1.topics_enabled': {},
  'privacy_sandbox.m1.fledge_enabled': {},
  'privacy_sandbox.m1.ad_measurement_enabled': {},
  'privacy_sandbox.m1.restricted': {},
  'privacy_sandbox.apis_enabled': {},
  'privacy_sandbox.apis_enabled_v2': {},
  'privacy_sandbox.manually_controlled_v2': {},
  'privacy_sandbox.page_viewed': {},
  'privacy_sandbox.topics_data_accessible_since':
      {logicalFn: timestampLogicalFn},
  'privacy_sandbox.blocked_topics': {},
  'privacy_sandbox.fledge_join_blocked': {},
  'privacy_sandbox.notice_displayed': {},
  'privacy_sandbox.consent_decision_made': {},
  'privacy_sandbox.no_confirmation_sandbox_disabled': {},
  'privacy_sandbox.no_confirmation_sandbox_restricted': {},
  'privacy_sandbox.no_confirmation_sandbox_managed': {},
  'privacy_sandbox.no_confirmation_3PC_blocked': {},
  'privacy_sandbox.no_confirmation_manually_controlled': {},
  'privacy_sandbox.disabled_insufficient_confirmation': {},
  'privacy_sandbox.first_party_sets_data_access_allowed_initialized': {},
  'privacy_sandbox.first_party_sets_enabled': {},
  'privacy_sandbox.topics_consent.consent_given': {},
  'privacy_sandbox.topics_consent.last_update_time':
      {logicalFn: timestampLogicalFn},
  'privacy_sandbox.topics_consent.last_update_reason': {},
  'privacy_sandbox.topics_consent.text_at_last_update': {},
  'privacy_sandbox.activity_type.record': {},
  'privacy_sandbox.activity_type.record2': {},
}));

function getPrefLogicalFn(prefName: string) {
  const all = new Map([
    ...advertisingPrefNames.entries(),
    ...trackingProtectionPrefNames.entries(),
    ...tpcdExperimentPrefs.entries(),
  ]);
  const config = all.get(prefName);
  if (config && config.logicalFn) {
    return config.logicalFn;
  }
  return defaultLogicalFn;
}

export class InternalsPage extends CustomElement implements RouteObserver {
  private browserProxy_: PrivacySandboxInternalsBrowserProxy =
      PrivacySandboxInternalsBrowserProxy.getInstance();
  whenLoaded: Promise<void>|null = null;

  static get is() {
    return 'internals-page';
  }

  constructor() {
    super();
    Router.getInstance().addObserver(this);
  }

  connectedCallback() {
    this.whenLoaded = this.load();
    const defaultPage =
        this.shadowRoot!.querySelector<HTMLElement>('[slot="tab"][selected]')
            ?.dataset['pageName']!;
    Router.getInstance().processInitialRoute(defaultPage);
  }

  static override get template() {
    return getTemplate();
  }

  disconnectedCallback() {
    Router.getInstance().removeObserver(this);
  }

  maybeAddPrefsToDom(parentElement: HTMLElement|null, prefNameList: string[]) {
    if (parentElement) {
      this.addPrefsToDom(parentElement, prefNameList);
    } else {
      console.error(
          'Parent element not defined for prefNameList:', prefNameList);
    }
  }

  addPrefsToDom(parentElement: HTMLElement, prefNameList: string[]) {
    const handler = this.browserProxy_.handler;
    prefNameList.forEach(async (prefName) => {
      const prefValue = await handler.readPref(prefName);
      const item = document.createElement('pref-display');
      parentElement.appendChild(item);
      item.configure(prefName, prefValue.s, getPrefLogicalFn(prefName));
    });
  }

  // Called when the route changes, this method updates the selected tab in the
  // UI to match the current page in the URL.
  onRouteChanged(pageName: string): void {
    const frameList =
        this.shadowRoot!.querySelector<CrFrameListElement>('#ps-page');
    if (!frameList) {
      return;
    }

    const allTabsInDom =
        Array.from(frameList.querySelectorAll<HTMLElement>('[slot="tab"]'));
    const index = allTabsInDom.findIndex(
        (tab: HTMLElement) => tab.dataset['pageName'] === pageName);

    if (index !== -1) {
      frameList.setAttribute('selected-index', index.toString());
    } else {
      frameList.setAttribute('selected-index', '0');
    }
  }

  async load() {
    this.maybeAddPrefsToDom(
        this.shadowRoot!.querySelector<HTMLElement>('#advertising-prefs-panel'),
        [...advertisingPrefNames.keys()]);
    this.maybeAddPrefsToDom(
        this.shadowRoot!.querySelector<HTMLElement>(
            '#tracking-protection-prefs-panel'),
        [...trackingProtectionPrefNames.keys()]);
    this.maybeAddPrefsToDom(
        this.shadowRoot!.querySelector<HTMLElement>(
            '#tpcd-experiment-prefs-panel'),
        [...tpcdExperimentPrefs.keys()]);

    const tabBox =
        this.shadowRoot!.querySelector<CrFrameListElement>('#ps-page')!;
    const csPanels = new Map<string, HTMLElement>();
    const handler = this.browserProxy_.handler;
    const shouldShowTpcdMetadataGrants =
        this.browserProxy_.shouldShowTpcdMetadataGrants();

    for (let i = ContentSettingsType.MIN_VALUE;
         i <= ContentSettingsType.MAX_VALUE; i++) {
      // Controls the visibility of the TPCD_METADATA_GRANTS tab.
      if (ContentSettingsType[i] === 'TPCD_METADATA_GRANTS' &&
          !shouldShowTpcdMetadataGrants) {
        continue;
      }
      const tab = document.createElement('div');
      tab.innerText = ContentSettingsType[i];
      tab.setAttribute('slot', 'tab');
      tab.dataset['pageName'] = ContentSettingsType[i].toLowerCase();
      tabBox.appendChild(tab);

      const panel = document.createElement('div');
      panel.setAttribute('slot', 'panel');
      panel.setAttribute('style', 'content-settings');
      panel.setAttribute('title', ContentSettingsType[i]);
      const panelTitle = document.createElement('h2');
      panelTitle.innerText = ContentSettingsType[i];
      panel.appendChild(panelTitle);
      tabBox.appendChild(panel);

      csPanels.set(ContentSettingsType[i], panel);
    }

    tabBox.addEventListener('selected-index-change', () => {
      const selectedTab =
          tabBox.querySelector<HTMLElement>('[slot="tab"][selected]');
      if (selectedTab?.dataset['pageName']) {
        Router.getInstance().navigateTo(selectedTab.dataset['pageName']);
      }
    });

    for (let i = ContentSettingsType.MIN_VALUE;
         i <= ContentSettingsType.MAX_VALUE; i++) {
      let mojoResponse;
      if (i === ContentSettingsType.TPCD_METADATA_GRANTS) {
        // Prevents the TPCD Metadata Grants tab from loading and rendering if
        // its flag is disabled.
        if (!shouldShowTpcdMetadataGrants) {
          continue;
        }
        // This one is special and can't be read through readContentSettings().
        mojoResponse = await handler.getTpcdMetadataGrants();
      } else {
        mojoResponse = await handler.readContentSettings(i);
      }
      mojoResponse.contentSettings.forEach((cs: any) => {
        const panel = csPanels.get(ContentSettingsType[i])!;
        const item = document.createElement('content-setting-pattern-source');
        panel.appendChild(item);
        item.configure(handler, cs);
        item.setAttribute('collapsed', 'true');
      });
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'internals-page': InternalsPage;
  }
}
customElements.define('internals-page', InternalsPage);
