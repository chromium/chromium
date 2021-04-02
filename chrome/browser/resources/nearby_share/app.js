// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The 'nearby-share' component is the entry point for the Nearby
 * Share flow. It is used as a standalone dialog via chrome://nearby and as part
 * of the ChromeOS share sheet.
 */

import 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';
import './shared/nearby_onboarding_page.m.js';
import './shared/nearby_visibility_page.m.js';
import './nearby_confirmation_page.js';
import './nearby_discovery_page.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {NearbyShareSettingsBehavior} from './shared/nearby_share_settings_behavior.m.js';

/** @enum {string} */
const Page = {
  CONFIRMATION: 'confirmation',
  DISCOVERY: 'discovery',
  ONBOARDING: 'onboarding',
  VISIBILITY: 'visibility',
};

Polymer({
  is: 'nearby-share-app',

  behaviors: [NearbyShareSettingsBehavior],

  _template: html`{__html_template__}`,

  properties: {
    /** Mirroring the enum so that it can be used from HTML bindings. */
    Page: {
      type: Object,
      value: Page,
    },

    /**
     * Set by the nearby-discovery-page component when switching to the
     * nearby-confirmation-page.
     * @private {?nearbyShare.mojom.ConfirmationManagerInterface}
     */
    confirmationManager_: {
      type: Object,
      value: null,
    },

    /**
     * Set by the nearby-discovery-page component when switching to the
     * nearby-confirmation-page.
     * @private {?nearbyShare.mojom.TransferUpdateListenerPendingReceiver}
     */
    transferUpdateListener_: {
      type: Object,
      value: null,
    },

    /**
     * The currently selected share target set by the nearby-discovery-page
     * component when the user selects a device.
     * @private {?nearbyShare.mojom.ShareTarget}
     */
    selectedShareTarget_: {
      type: Object,
      value: null,
    },

    /**
     * Preview info of attachment to be sent, set by the nearby-discovery-page.
     * @private {?nearbyShare.mojom.PayloadPreview}
     */
    payloadPreview_: {
      type: Object,
      value: null,
    },
  },

  listeners: {
    'change-page': 'onChangePage_',
    'close': 'onClose_',
    'onboarding-complete': 'onOnboardingComplete_',
  },

  /**
   * @return {!CrViewManagerElement} the view manager
   * @private
   */
  getViewManager_() {
    return /** @type {!CrViewManagerElement} */ (this.$.viewManager);
  },

  /**
   * Called when component is attached and all settings values have been
   * retrieved.
   */
  onSettingsRetrieved() {
    if (this.settings.isOnboardingComplete) {
      if (!this.settings.enabled) {
        // When a new share is triggered, if the user has completed onboarding
        // previously, then silently enable the feature and continue to
        // discovery page directly.
        this.set('settings.enabled', true);
      }
      this.getViewManager_().switchView(Page.DISCOVERY);
    } else {
      this.getViewManager_().switchView(Page.ONBOARDING);
    }
  },

  /**
   * Handler for the change-page event.
   * @param {!CustomEvent<!{page: Page}>} event
   * @private
   */
  onChangePage_(event) {
    this.getViewManager_().switchView(event.detail.page);
  },

  /**
   * Handler for the close event.
   * @param {!Event} event
   * @private
   */
  onClose_(event) {
    chrome.send('close');
  },

  /**
   * Handler for when onboarding is completed.
   * @param {!Event} event
   * @private
   */
  onOnboardingComplete_(event) {
    this.getViewManager_().switchView(Page.DISCOVERY);
  },
});
