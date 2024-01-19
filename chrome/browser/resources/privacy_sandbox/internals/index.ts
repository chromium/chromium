// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import './content_setting_pattern_source.js';
import './pref_display.js';
import './mojo_timedelta.js';
import 'chrome://resources/cr_elements/cr_tab_box/cr_tab_box.js';

import {PageHandler, PageHandlerRemote} from './privacy_sandbox_internals.mojom-webui.js';
import {defaultLogicalFn, LogicalFn, timestampLogicalFn} from './value_display.js';

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

const trackingProtectionPrefNames: Map<string, PrefConfig> =
    new Map(Object.entries({
      'tracking_protection.ip_protection_enabled': {},
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

class DataLoader {
  pageHandler: PageHandlerRemote;

  constructor(handler: PageHandlerRemote) {
    this.pageHandler = handler;
  }

  async maybeAddPrefsToDom(
      parentElement: HTMLElement|null, prefNameList: string[]) {
    if (parentElement) {
      this.addPrefsToDom(parentElement, prefNameList);
    } else {
      console.error(
          'Parent element not defined for prefNameList:', prefNameList);
    }
  }

  async addPrefsToDom(parentElement: HTMLElement, prefNameList: string[]) {
    prefNameList.forEach(async (prefName) => {
      const prefValue = await this.pageHandler.readPref(prefName);
      const item = document.createElement('pref-display');
      parentElement.appendChild(item);
      item.configure(prefName, prefValue.s, getPrefLogicalFn(prefName));
    });
  }

  async load() {
    const cookieParent =
        document.querySelector<HTMLElement>('#cookie-content-settings')!;
    const cookieSettings = await this.pageHandler.getCookieSettings();
    cookieSettings.contentSettings.forEach((cs) => {
      const item = document.createElement('content-setting-pattern-source');
      cookieParent.appendChild(item);
      item.configure(this.pageHandler, cs);
      item.setAttribute('collapsed', 'true');
    });

    const tpcdMetadataParent =
        document.querySelector<HTMLElement>('#tpcd-metadata-grants')!;
    const tpcdMetadataGrants = await this.pageHandler.getTpcdMetadataGrants();
    tpcdMetadataGrants.contentSettings.forEach((cs) => {
      const item = document.createElement('content-setting-pattern-source');
      tpcdMetadataParent.appendChild(item);
      item.configure(this.pageHandler, cs);
      item.setAttribute('collapsed', 'true');
    });

    const tpcdHeuristicsParent =
        document.querySelector<HTMLElement>('#tpcd-heuristics-grants')!;
    const tpcdHeuristicsGrants =
        await this.pageHandler.getTpcdHeuristicsGrants();
    tpcdHeuristicsGrants.contentSettings.forEach((cs) => {
      const item = document.createElement('content-setting-pattern-source');
      tpcdHeuristicsParent.appendChild(item);
      item.configure(this.pageHandler, cs);
      item.setAttribute('collapsed', 'true');
    });

    const tpcdTrialParent = document.querySelector<HTMLElement>('#tpcd-trial')!;
    const tpcdTrial = await this.pageHandler.getTpcdTrial();
    tpcdTrial.contentSettings.forEach((cs) => {
      const item = document.createElement('content-setting-pattern-source');
      tpcdTrialParent.appendChild(item);
      item.configure(this.pageHandler, cs);
      item.setAttribute('collapsed', 'true');
    });

    const topLevelTpcdTrialParent =
        document.querySelector<HTMLElement>('#top-level-tpcd-trial')!;
    const topLevelTpcdTrial = await this.pageHandler.getTopLevelTpcdTrial();
    topLevelTpcdTrial.contentSettings.forEach((cs) => {
      const item = document.createElement('content-setting-pattern-source');
      topLevelTpcdTrialParent.appendChild(item);
      item.configure(this.pageHandler, cs);
      item.setAttribute('collapsed', 'true');
    });

    this.maybeAddPrefsToDom(
        document.querySelector<HTMLElement>('#advertising-prefs'),
        [...advertisingPrefNames.keys()]);
    this.maybeAddPrefsToDom(
        document.querySelector<HTMLElement>('#tracking-protection-prefs'),
        [...trackingProtectionPrefNames.keys()]);
    this.maybeAddPrefsToDom(
        document.querySelector<HTMLElement>('#tpcd-experiment-prefs'),
        [...tpcdExperimentPrefs.keys()]);
  }
}

document.addEventListener('DOMContentLoaded', () => {
  const loader = new DataLoader(PageHandler.getRemote());
  loader.load();
});
