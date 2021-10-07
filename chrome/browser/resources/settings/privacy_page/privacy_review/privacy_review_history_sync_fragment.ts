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

import {assert} from 'chrome://resources/js/assert.m.js';
import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BaseMixin, BaseMixinInterface} from '../../base_mixin.js';
import {SyncBrowserProxy, SyncBrowserProxyImpl, SyncPrefs} from '../../people_page/sync_browser_proxy.js';

const PrivacyReviewHistorySyncFragmentElementBase =
    mixinBehaviors([WebUIListenerBehavior], BaseMixin(PolymerElement)) as
    {new (): PolymerElement & WebUIListenerBehavior & BaseMixinInterface};

/** @polymer */
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
  private historySyncVirtualPref_: chrome.settingsPrivate.PrefObject;

  ready() {
    super.ready();

    this.addWebUIListener(
        'sync-prefs-changed',
        (syncPrefs: SyncPrefs) => this.onSyncPrefsChange_(syncPrefs));
    this.syncBrowserProxy_.sendSyncPrefsChanged();
  }

  private onSyncPrefsChange_(syncPrefs: SyncPrefs) {
    this.syncPrefs_ = syncPrefs;
    this.set(
        'historySyncVirtualPref_.value',
        this.syncPrefs_.syncAllDataTypes || this.syncPrefs_.typedUrlsSynced);
  }

  private onToggleClick_() {
    this.syncPrefs_.typedUrlsSynced = this.historySyncVirtualPref_.value;
    // If the user disabled history sync, then sync all also needs to be off.
    if (!this.syncPrefs_.typedUrlsSynced) {
      this.syncPrefs_.syncAllDataTypes = false;
    }
    this.syncBrowserProxy_.setSyncDatatypes(this.syncPrefs_);
  }
}

customElements.define(
    PrivacyReviewHistorySyncFragmentElement.is,
    PrivacyReviewHistorySyncFragmentElement);
