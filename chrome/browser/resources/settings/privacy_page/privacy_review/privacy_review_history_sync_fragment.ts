// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'privacy-review-history-sync-fragment' is the fragment in a privacy review
 * card that contains the history sync setting and its description.
 */
import '../../prefs/prefs.js';
import './privacy_review_description_item.js';
import './privacy_review_fragment_shared_css.js';
import './privacy_review_fragment_shared_css.js';
import '../../controls/settings_toggle_button.js';

import {WebUIListenerMixin, WebUIListenerMixinInterface} from 'chrome://resources/js/web_ui_listener_mixin.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BaseMixin} from '../../base_mixin.js';
import {SettingsToggleButtonElement} from '../../controls/settings_toggle_button.js';
import {SyncBrowserProxy, SyncBrowserProxyImpl, SyncPrefs, syncPrefsIndividualDataTypes} from '../../people_page/sync_browser_proxy.js';
import {routes} from '../../route.js';
import {Route, RouteObserverMixin, RouteObserverMixinInterface, Router} from '../../router.js';

import {PrivacyReviewStep} from './constants.js';

export interface PrivacyReviewHistorySyncFragmentElement {
  $: {
    historyToggle: SettingsToggleButtonElement,
  };
}

const PrivacyReviewHistorySyncFragmentElementBase =
    RouteObserverMixin(WebUIListenerMixin(BaseMixin(PolymerElement))) as {
  new (): PolymerElement&RouteObserverMixinInterface&
      WebUIListenerMixinInterface;
};

export class PrivacyReviewHistorySyncFragmentElement extends
    PrivacyReviewHistorySyncFragmentElementBase {
  static get is() {
    return 'privacy-review-history-sync-fragment';
  }

  static get template() {
    return html`{__html_template__}`;
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
  private historySyncVirtualPref_: chrome.settingsPrivate.PrefObject;

  ready() {
    super.ready();

    this.addWebUIListener(
        'sync-prefs-changed',
        (syncPrefs: SyncPrefs) => this.onSyncPrefsChange_(syncPrefs));
    this.syncBrowserProxy_.sendSyncPrefsChanged();
  }

  currentRouteChanged(newRoute: Route) {
    if (newRoute === routes.PRIVACY_REVIEW &&
        Router.getInstance().getQueryParameters().get('step') ===
            PrivacyReviewStep.HISTORY_SYNC) {
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
  }

  private onToggleClick_() {
    this.syncPrefs_.typedUrlsSynced = this.historySyncVirtualPref_.value;
    this.syncPrefs_.syncAllDataTypes = this.shouldSyncAllBeOn_();
    this.syncBrowserProxy_.setSyncDatatypes(this.syncPrefs_);
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
    'privacy-review-history-sync-fragment':
        PrivacyReviewHistorySyncFragmentElement;
  }
}

customElements.define(
    PrivacyReviewHistorySyncFragmentElement.is,
    PrivacyReviewHistorySyncFragmentElement);
