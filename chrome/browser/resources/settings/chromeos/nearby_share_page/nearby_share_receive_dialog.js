// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'nearby-share-receive-dialog' shows two main pages:
 *  - high visibility receive page
 *  - Non-contact confirm page (contacts are confirmed w/ a notification)
 *
 * This dialog also supports showing the onboarding flow and will automatically
 * show onboarding if the feature is turned off and one of the two main pages is
 * requested.
 *
 * By default this dialog will not show anything until the caller calls one of
 * the following:
 *  - showOnboarding()
 *  - showHighVisibilityPage()
 *  - showConfirmPage()
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';
import '../../prefs/prefs.js';
import '../../shared/nearby_onboarding_one_page.js';
import '../../shared/nearby_onboarding_page.js';
import '../../shared/nearby_visibility_page.js';
import './nearby_share_confirm_page.js';
import './nearby_share_high_visibility_page.js';

import {ReceiveManagerInterface, ReceiveObserverInterface, ReceiveObserverReceiver, RegisterReceiveSurfaceResult, ShareTarget, TransferMetadata, TransferStatus} from '/mojo/nearby_share.mojom-webui.js';
import {assert} from 'chrome://resources/ash/common/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {NearbySettings} from '../../shared/nearby_share_settings_behavior.js';

import {getTemplate} from './nearby_share_receive_dialog.html.js';
import {getReceiveManager, observeReceiveManager} from './nearby_share_receive_manager.js';

/** @enum {string} */
const Page = {
  HIGH_VISIBILITY: 'high-visibility',
  CONFIRM: 'confirm',
  ONBOARDING: 'onboarding',
  ONEPAGE_ONBOARDING: 'onboarding-one',
  VISIBILITY: 'visibility',
};

/** @polymer */
class NearbyShareReceiveDialogElement extends PolymerElement {
  static get is() {
    return 'nearby-share-receive-dialog';
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

      /** @type {?ShareTarget} */
      shareTarget: {
        type: Object,
        value: null,
      },

      /** @type {?string} */
      connectionToken: {
        type: String,
        value: null,
      },

      /** @type {NearbySettings} */
      settings: {
        type: Object,
        notify: true,
        value: {},
      },

      /** @private */
      isSettingsRetreived: {
        type: Boolean,
        value: false,
      },

      /**
       * Status of the current transfer.
       * @private {?TransferStatus}
       */
      transferStatus_: {
        type: TransferStatus,
        value: null,
      },

      /**
       * @private {boolean}
       */
      nearbyProcessStopped_: {
        type: Boolean,
        value: false,
      },

      /**
       * @private {boolean}
       */
      startAdvertisingFailed_: {
        type: Boolean,
        value: false,
      },
    };
  }

  static get observers() {
    return [
      'onSettingsLoaded_(isSettingsRetreived)',
    ];
  }

  constructor() {
    super();

    /** @private {boolean} */
    this.closing_ = false;

    /**
     * What should happen once we get settings values from mojo.
     * @private {?function()}
     * */
    this.postSettingsCallback = null;

    /**
     * What should happen once onboarding is complete.
     * @private {?function()}
     * */
    this.postOnboardingCallback = null;

    /** @private {?ReceiveManagerInterface} */
    this.receiveManager_ = null;

    /** @private {?ReceiveObserverReceiver} */
    this.observerReceiver_ = null;

    /**
     * Timestamp in milliseconds since unix epoch of when high visibility will
     * be turned off.
     * @private {number}
     */
    this.highVisibilityShutoffTimestamp_ = 0;

    /** @private {?RegisterReceiveSurfaceResult} */
    this.registerForegroundReceiveSurfaceResult_ = null;
  }

  /** @override */
  ready() {
    super.ready();

    this.addEventListener('accept', this.onAccept_);
    this.addEventListener('cancel', this.onCancel_);
    this.addEventListener('change-page', (event) => {
      this.onChangePage_(
          /** @type {!CustomEvent<!{page: Page}>} */ (event));
    });
    this.addEventListener('onboarding-complete', this.onOnboardingComplete_);
    this.addEventListener('reject', this.onReject_);
    this.addEventListener('close', this.close_);
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();

    this.closing_ = false;
    this.receiveManager_ = getReceiveManager();
    this.observerReceiver_ = observeReceiveManager(
        /** @type {!ReceiveObserverInterface} */ (this));
  }

  /** @override */
  disconnectedCallback() {
    super.disconnectedCallback();

    if (this.observerReceiver_) {
      this.observerReceiver_.$.close();
    }
  }

  /**
   * Records via Standard Feature Usage Logging whether or not advertising
   * successfully starts when the user clicks the "Device nearby is sharing"
   * notification.
   * @param {boolean} success
   * @private
   */
  recordFastInitiationNotificationUsage_(success) {
    const url = new URL(document.URL);
    const urlParams = new URLSearchParams(url.search);
    if (urlParams.get('entrypoint') === 'notification') {
      this.receiveManager_.recordFastInitiationNotificationUsage(success);
    }
  }

  /**
   * Determines if the feature flag for One-page onboarding workflow is enabled.
   * @return {boolean} whether the new one-page onboarding workflow is enabled
   * @private
   */
  isOnePageOnboardingEnabled_() {
    return loadTimeData.getBoolean('isOnePageOnboardingEnabled');
  }

  /**
   * Mojo callback when high visibility changes. If high visibility is false
   * due to a user cancel, we force this dialog to close as well.
   * @param {boolean} inHighVisibility
   */
  onHighVisibilityChanged(inHighVisibility) {
    const now = performance.now();

    if (inHighVisibility === false &&
        now < this.highVisibilityShutoffTimestamp_ &&
        this.transferStatus_ !== TransferStatus.kAwaitingLocalConfirmation) {
      this.close_();
      return;
    }

    // If high visibility has been attained, then the process must be up and
    // advertising must be on.
    if (inHighVisibility) {
      this.startAdvertisingFailed_ = false;
      this.nearbyProcessStopped_ = false;
      this.recordFastInitiationNotificationUsage_(/*success=*/ true);
    }
  }

  /**
   * Mojo callback when transfer status changes.
   * @param {!ShareTarget} shareTarget
   * @param {!TransferMetadata} metadata
   */
  onTransferUpdate(shareTarget, metadata) {
    this.transferStatus_ = metadata.status;

    if (metadata.status === TransferStatus.kAwaitingLocalConfirmation) {
      this.shareTarget = shareTarget;
      this.connectionToken =
          (metadata && metadata.token) ? metadata.token : null;
      this.showConfirmPage();
    }
  }

  /**
   * Mojo callback when the Nearby utility process stops.
   */
  onNearbyProcessStopped() {
    this.nearbyProcessStopped_ = true;
  }

  /**
   * Mojo callback when advertising fails to start.
   */
  onStartAdvertisingFailure() {
    this.startAdvertisingFailed_ = true;
    this.recordFastInitiationNotificationUsage_(/*success=*/ false);
  }

  /**
   * @private
   */
  onSettingsLoaded_() {
    if (this.postSettingsCallback) {
      this.postSettingsCallback();
      this.postSettingsCallback = null;
    }
  }

  /**
   * @return {!CrViewManagerElement} the view manager
   * @private
   */
  getViewManager_() {
    return /** @type {!CrViewManagerElement} */ (this.$.viewManager);
  }

  /** @private */
  close_() {
    // If we are already waiting for high visibility to exit, then we don't need
    // to trigger it again.
    if (this.closing_) {
      return;
    }

    this.closing_ = true;
    this.receiveManager_.unregisterForegroundReceiveSurface().then(() => {
      const dialog = /** @type {!CrDialogElement} */ (this.$.dialog);
      if (dialog.open) {
        dialog.close();
      }
    });
  }

  /**
   * Defers running a callback for page navigation in the case that we do not
   * yet have a settings.enabled value from mojo or if Nearby Share is not
   * enabled yet and we need to run the onboarding flow first.
   * @param {function()} callback
   * @return {boolean} true if the callback has been scheduled for later, false
   *     if it did not need to be deferred and can be called now.
   */
  deferCallIfNecessary(callback) {
    if (!this.isSettingsRetreived) {
      // Let onSettingsLoaded_ handle the navigation because we don't know yet
      // if the feature is enabled and we might need to show onboarding.
      this.postSettingsCallback = callback;
      return true;
    }

    if (!this.settings.isOnboardingComplete) {
      // We need to show onboarding first if onboarding is not yet complete, but
      // we need to run the callback afterward.
      this.postOnboardingCallback = callback;
      if (this.isOnePageOnboardingEnabled_()) {
        this.getViewManager_().switchView(Page.ONEPAGE_ONBOARDING);
      } else {
        this.getViewManager_().switchView(Page.ONBOARDING);
      }
      return true;
    }

    // If onboarding is already complete but Nearby is disabled we re-enable
    // Nearby.
    if (!this.settings.enabled) {
      this.set('settings.enabled', true);
    }

    // We know the feature is enabled so no need to defer the call.
    return false;
  }

  /**
   * Call to show the onboarding flow and then close when complete.
   */
  showOnboarding() {
    // Setup the callback to close this dialog when onboarding is complete.
    this.postOnboardingCallback = this.close_.bind(this);
    if (this.isOnePageOnboardingEnabled_()) {
      this.getViewManager_().switchView(Page.ONEPAGE_ONBOARDING);
    } else {
      this.getViewManager_().switchView(Page.ONBOARDING);
    }
  }

  /**
   * Call to show the high visibility page.
   * @param {number} shutoffTimeoutInSeconds Duration of the high
   *     visibility session, after which the session would be turned off.
   */
  showHighVisibilityPage(shutoffTimeoutInSeconds) {
    // Check if we need to wait for settings values from mojo or if we need to
    // run onboarding first before showing the page.
    if (this.deferCallIfNecessary(
            this.showHighVisibilityPage.bind(this, shutoffTimeoutInSeconds))) {
      return;
    }

    // performance.now() returns DOMHighResTimeStamp in milliseconds.
    this.highVisibilityShutoffTimestamp_ =
        performance.now() + (shutoffTimeoutInSeconds * 1000);

    // Register a receive surface to enter high visibility and show the page.
    this.receiveManager_.registerForegroundReceiveSurface().then((result) => {
      this.registerForegroundReceiveSurfaceResult_ = result.result;
      this.getViewManager_().switchView(Page.HIGH_VISIBILITY);
    });
  }

  /**
   * Call to show the share target configuration page.
   */
  showConfirmPage() {
    // Check if we need to wait for settings values from mojo or if we need to
    // run onboarding first before showing the page.
    if (this.deferCallIfNecessary(this.showConfirmPage.bind(this))) {
      return;
    }
    this.getViewManager_().switchView(Page.CONFIRM);
  }

  /**
   * Child views can fire a 'change-page' event to trigger a page change.
   * @param {!CustomEvent<!{page: Page}>} event
   * @private
   */
  onChangePage_(event) {
    this.getViewManager_().switchView(event.detail.page);
  }

  /** @private */
  onCancel_() {
    this.close_();
  }

  /** @private */
  onAccept_() {
    assert(this.shareTarget);
    this.receiveManager_.accept(this.shareTarget.id).then((success) => {
      if (success) {
        this.close_();
      } else {
        // TODO(vecore): Show error state.
        this.close_();
      }
    });
  }

  /** @private */
  onOnboardingComplete_() {
    if (!this.postOnboardingCallback) {
      return;
    }

    this.postOnboardingCallback();
    this.postOnboardingCallback = null;
  }

  /** @private */
  onReject_() {
    assert(this.shareTarget);
    this.receiveManager_.reject(this.shareTarget.id).then((success) => {
      if (success) {
        this.close_();
      } else {
        // TODO(vecore): Show error state.
        this.close_();
      }
    });
  }
}

customElements.define(
    NearbyShareReceiveDialogElement.is, NearbyShareReceiveDialogElement);
