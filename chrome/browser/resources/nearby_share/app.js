// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shared/nearby_onboarding_one_page.js';
import './shared/nearby_onboarding_page.js';
import './shared/nearby_visibility_page.js';
import './nearby_confirmation_page.js';
import './nearby_discovery_page.js';
import 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';
import {NearbyShareSettingsBehavior, NearbyShareSettingsBehaviorInterface} from './shared/nearby_share_settings_behavior.js';
import {CloseReason} from './shared/types.js';

/**
 * @fileoverview The 'nearby-share' component is the entry point for the Nearby
 * Share flow. It is used as a standalone dialog via chrome://nearby and as part
 * of the ChromeOS share sheet.
 */

/** @enum {string} */
const Page = {
  CONFIRMATION: 'confirmation',
  DISCOVERY: 'discovery',
  ONBOARDING: 'onboarding',
  ONEPAGE_ONBOARDING: 'onboarding-one',
  VISIBILITY: 'visibility',
};

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {NearbyShareSettingsBehaviorInterface}
 */
const NearbyShareAppElementBase =
    mixinBehaviors([NearbyShareSettingsBehavior], PolymerElement);

/** @polymer */
export class NearbyShareAppElement extends NearbyShareAppElementBase {
  static get is() {
    return 'nearby-share-app';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
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
       * Preview info of attachment to be sent, set by the
       * nearby-discovery-page.
       * @private {?nearbyShare.mojom.PayloadPreview}
       */
      payloadPreview_: {
        type: Object,
        value: null,
      },
    };
  }

  /** @override */
  ready() {
    super.ready();

    this.addEventListener(
        'change-page',
        e =>
            this.onChangePage_(/** @type {!CustomEvent<!{page: Page}>} */ (e)));
    this.addEventListener(
        'close',
        e => this.onClose_(
            /** @type {!CustomEvent<!{reason: CloseReason}>} */ (e)));
    this.addEventListener('onboarding-complete', this.onOnboardingComplete_);
  }

  /**
   * @return {!CrViewManagerElement} the view manager
   * @private
   */
  getViewManager_() {
    return /** @type {!CrViewManagerElement} */ (this.$.viewManager);
  }

  /**
   * Called whenever view changes.
   * ChromeVox screen reader requires focus on #pageContainer to read
   * dialog.
   * @param {string} page
   * @private
   */
  focusOnPageContainer_(page) {
    this.shadowRoot.querySelector(`nearby-${page}-page`)
        .shadowRoot.querySelector('nearby-page-template')
        .shadowRoot.querySelector('#pageContainer')
        .focus();
  }

  /**
   * Determines if the feature flag for One-page onboarding workflow is enabled.
   * @return {boolean} whether the one-page onboarding is enabled
   * @private
   */
  isOnePageOnboardingEnabled_() {
    return loadTimeData.getBoolean('isOnePageOnboardingEnabled');
  }

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
      this.focusOnPageContainer_(Page.DISCOVERY);

      return;
    }

    const onboardingPage = this.isOnePageOnboardingEnabled_() ?
        Page.ONEPAGE_ONBOARDING :
        Page.ONBOARDING;
    this.getViewManager_().switchView(onboardingPage);
    this.focusOnPageContainer_(onboardingPage);
  }

  /**
   * Handler for the change-page event.
   * @param {!CustomEvent<!{page: Page}>} event
   * @private
   */
  onChangePage_(event) {
    this.getViewManager_().switchView(event.detail.page);
    this.focusOnPageContainer_(event.detail.page);
  }

  /**
   * Handler for the close event.
   * @param {!CustomEvent<!{reason: CloseReason}>} event
   * @private
   */
  onClose_(event) {
    // TODO(b/237796007): Handle the case of null |event.detail|
    const reason =
        event.detail.reason == null ? CloseReason.UNKNOWN : event.detail.reason;
    chrome.send('close', [reason]);
  }

  /**
   * Handler for when onboarding is completed.
   * @param {!Event} event
   * @private
   */
  onOnboardingComplete_(event) {
    this.getViewManager_().switchView(Page.DISCOVERY);
    this.focusOnPageContainer_(Page.DISCOVERY);
  }
}

customElements.define(NearbyShareAppElement.is, NearbyShareAppElement);
