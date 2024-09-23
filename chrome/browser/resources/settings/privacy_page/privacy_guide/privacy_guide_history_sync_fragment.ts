// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'privacy-guide-history-sync-fragment' is the fragment in a privacy guide
 * card that contains the history sync setting and its description.
 */
import '/shared/settings/prefs/prefs.js';
import './privacy_guide_description_item.js';
import './privacy_guide_fragment_shared.css.js';
import './privacy_guide_fragment_shared.css.js';
import '../../controls/settings_toggle_button.js';

import type {SyncBrowserProxy, SyncPrefs} from '/shared/settings/people_page/sync_browser_proxy.js';
import {SyncBrowserProxyImpl, syncPrefsIndividualDataTypes} from '/shared/settings/people_page/sync_browser_proxy.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BaseMixin} from '../../base_mixin.js';
import type {SettingsToggleButtonElement} from '../../controls/settings_toggle_button.js';
import type {MetricsBrowserProxy} from '../../metrics_browser_proxy.js';
import {MetricsBrowserProxyImpl, PrivacyGuideSettingsStates, PrivacyGuideStepsEligibleAndReached} from '../../metrics_browser_proxy.js';
import {routes} from '../../route.js';
import type {Route} from '../../router.js';
import {RouteObserverMixin, Router} from '../../router.js';

import {PrivacyGuideStep} from './constants.js';
import {getTemplate} from './privacy_guide_history_sync_fragment.html.js';

export interface PrivacyGuideHistorySyncFragmentElement {
  $: {
    historyToggle: SettingsToggleButtonElement,
  };
}

const PrivacyGuideHistorySyncFragmentElementBase =
    RouteObserverMixin(WebUiListenerMixin(BaseMixin(PolymerElement)));

export class PrivacyGuideHistorySyncFragmentElement extends
    PrivacyGuideHistorySyncFragmentElementBase {
  static get is() {
    return 'privacy-guide-history-sync-fragment';
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

      /** Virtual pref to drive the settings-toggle from syncPrefs. */
      historySyncVirtualPref_: {
        type: Object,
        notify: true,
        value() {
          return {
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: false,
          };
        },
      },
    };
  }

  private syncBrowserProxy_: SyncBrowserProxy =
      SyncBrowserProxyImpl.getInstance();
  private syncPrefs_: SyncPrefs;
  /*
   * |null| indicates that the value is currently unknown and that it will be
   * set with the next sync prefs update.
   */
  private syncAllCache_: boolean|null = null;
  private historySyncVirtualPref_: chrome.settingsPrivate.PrefObject<boolean>;
  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();
  private startStateHistorySyncOn_: boolean;
  /*
   * This is needed as the nature of SyncPrefs means there is a chance they are
   * not actually initialized before view-enter-start, so the pref value is read
   * when the page fires its on-load update/initialization of SyncPrefs.
   */
  private firstSyncPrefUpdate_: boolean = true;

  override ready() {
    super.ready();
    this.addEventListener('view-enter-start', this.onViewEnterStart_);
    this.addEventListener('view-exit-finish', this.onViewExitFinish_);

    this.addWebUiListener(
        'sync-prefs-changed',
        (syncPrefs: SyncPrefs) => this.onSyncPrefsChange_(syncPrefs));
    this.syncBrowserProxy_.sendSyncPrefsChanged();
  }

  override focus() {
    // The fragment element is focused when it becomes visible. Move the focus
    // to the fragment header, so that the newly shown content of the fragment
    // is downwards from the focus position. This allows users of screen readers
    // to continue navigating the screen reader position downwards through the
    // newly visible content.
    this.shadowRoot!.querySelector<HTMLElement>('[focus-element]')!.focus();
  }

  private onViewEnterStart_() {
    this.metricsBrowserProxy_
        .recordPrivacyGuideStepsEligibleAndReachedHistogram(
            PrivacyGuideStepsEligibleAndReached.HISTORY_SYNC_REACHED);
  }

  private onViewExitFinish_() {
    const endStateHistorySyncOn = this.syncPrefs_.typedUrlsSynced;
    let state: PrivacyGuideSettingsStates|null = null;
    if (this.startStateHistorySyncOn_) {
      state = endStateHistorySyncOn ?
          PrivacyGuideSettingsStates.HISTORY_SYNC_ON_TO_ON :
          PrivacyGuideSettingsStates.HISTORY_SYNC_ON_TO_OFF;
    } else {
      state = endStateHistorySyncOn ?
          PrivacyGuideSettingsStates.HISTORY_SYNC_OFF_TO_ON :
          PrivacyGuideSettingsStates.HISTORY_SYNC_OFF_TO_OFF;
    }
    this.metricsBrowserProxy_.recordPrivacyGuideSettingsStatesHistogram(state!);

    this.firstSyncPrefUpdate_ = true;
  }

  override currentRouteChanged(newRoute: Route) {
    if (newRoute === routes.PRIVACY_GUIDE &&
        Router.getInstance().getQueryParameters().get('step') ===
            PrivacyGuideStep.HISTORY_SYNC) {
      // Sync all should not be re-enabled via the history sync card if there
      // was a navigation since caching sync all.
      this.syncAllCache_ = null;
    }
  }

  private onSyncPrefsChange_(syncPrefs: SyncPrefs) {
    this.syncPrefs_ = syncPrefs;
    if (this.syncAllCache_ === null) {
      this.syncAllCache_ = this.syncPrefs_.syncAllDataTypes;
    }
    this.set(
        'historySyncVirtualPref_.value',
        this.syncPrefs_.syncAllDataTypes || this.syncPrefs_.typedUrlsSynced);

    if (this.firstSyncPrefUpdate_) {
      this.startStateHistorySyncOn_ = this.syncPrefs_.typedUrlsSynced;
      this.firstSyncPrefUpdate_ = false;
    }
  }

  private onToggleClick_() {
    this.syncPrefs_.typedUrlsSynced = this.historySyncVirtualPref_.value;
    this.syncPrefs_.syncAllDataTypes = this.shouldSyncAllBeOn_();
    this.syncBrowserProxy_.setSyncDatatypes(this.syncPrefs_);
    if (this.syncPrefs_.typedUrlsSynced) {
      this.metricsBrowserProxy_.recordAction(
          'Settings.PrivacyGuide.ChangeHistorySyncOn');
    } else {
      this.metricsBrowserProxy_.recordAction(
          'Settings.PrivacyGuide.ChangeHistorySyncOff');
    }
  }

  /**
   * If sync all was on when the user reached the history sync card, then
   * disabling and re-enabling history sync while on the card should also
   * re-enable sync all in case all other sync datatypes are also still on.
   */
  private shouldSyncAllBeOn_(): boolean {
    if (!this.syncAllCache_) {
      return false;
    }
    for (const datatype of syncPrefsIndividualDataTypes) {
      if ((this.syncPrefs_ as {[key: string]: any})[datatype]) {
        continue;
      }
      if (datatype === 'wifiConfigurationsSynced' &&
          !this.syncPrefs_.wifiConfigurationsRegistered) {
        // Non-CrOS: |wifiConfigurationsRegistered| is false.
        // CrOS: If |wifiConfigurationsRegistered| is false then
        // |wifiConfigurationsSynced| is not shown in the advanced sync
        // controls UI. Hence it being false doesn't prevent re-enabling
        // sync all.
        continue;
      }
      return false;
    }
    return true;
  }
}
declare global {
  interface HTMLElementTagNameMap {
    'privacy-guide-history-sync-fragment':
        PrivacyGuideHistorySyncFragmentElement;
  }
}

customElements.define(
    PrivacyGuideHistorySyncFragmentElement.is,
    PrivacyGuideHistorySyncFragmentElement);
