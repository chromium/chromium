// Copyright 2020 The Chromium Authors. All rights reserved.
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

/** @enum {string} */
const Page = {
  HIGH_VISIBILITY: 'high-visibility',
  CONFIRM: 'confirm',
  ONBOARDING: 'onboarding',
  VISIBILITY: 'visibility',
};

Polymer({
  is: 'nearby-share-receive-dialog',

  properties: {
    /** Mirroring the enum so that it can be used from HTML bindings. */
    Page: {
      type: Object,
      value: Page,
    },

    /** @type {?nearbyShare.mojom.ShareTarget} */
    shareTarget: {
      type: Object,
      value: null,
    },

    /** @type {?string} */
    connectionToken: {
      type: String,
      value: null,
    },

    /** @type {nearby_share.NearbySettings} */
    settings: {
      type: Object,
      value: {},
    },

    /**
     * Status of the current transfer.
     * @private {?nearbyShare.mojom.TransferStatus}
     */
    transferStatus_: {
      type: nearbyShare.mojom.TransferStatus,
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
  },

  listeners: {
    'accept': 'onAccept_',
    'cancel': 'onCancel_',
    'change-page': 'onChangePage_',
    'onboarding-complete': 'onOnboardingComplete_',
    'reject': 'onReject_',
    'close': 'close_',
  },

  observers: [
    'onSettingsChanged_(settings.*)',
  ],

  /** @private {boolean} */
  closing_: false,

  /**
   * What should happen once we get settings values from mojo.
   * @private {?function()}
   * */
  postSettingsCallback: null,

  /**
   * What should happen once onboarding is complete.
   * @private {?function()}
   * */
  postOnboardingCallback: null,

  /** @private {?nearbyShare.mojom.ReceiveManagerInterface} */
  receiveManager_: null,

  /** @private {?nearbyShare.mojom.ReceiveObserverReceiver} */
  observerReceiver_: null,

  /**
   * Timestamp in milliseconds since unix epoch of when high visibility will
   * be turned off.
   * @private {number}
   */
  highVisibilityShutoffTimestamp_: 0,

  /** @private {?nearbyShare.mojom.RegisterReceiveSurfaceResult} */
  registerForegroundReceiveSurfaceResult_: null,

  /** @override */
  attached() {
    this.closing_ = false;
    this.errorString = null;
    this.receiveManager_ = nearby_share.getReceiveManager();
    this.observerReceiver_ = nearby_share.observeReceiveManager(
        /** @type {!nearbyShare.mojom.ReceiveObserverInterface} */ (this));
  },

  /** @override */
  detached() {
    if (this.observerReceiver_) {
      this.observerReceiver_.$.close();
    }
  },

  /**
   * Mojo callback when high visibility changes. If high visibility is false
   * due to a user cancel, we force this dialog to close as well.
   * @param {boolean} inHighVisibility
   */
  onHighVisibilityChanged(inHighVisibility) {
    const now = performance.now();

    if (inHighVisibility === false &&
        now < this.highVisibilityShutoffTimestamp_ &&
        this.transferStatus_ !==
            nearbyShare.mojom.TransferStatus.kAwaitingLocalConfirmation) {
      this.close_();
      return;
    }

    // If high visibility has been attained, then the process must be up and
    // advertising must be on.
    if (inHighVisibility) {
      this.startAdvertisingFailed_ = false;
      this.nearbyProcessStopped_ = false;
    }
  },

  /**
   * Mojo callback when transfer status changes.
   * @param {!nearbyShare.mojom.ShareTarget} shareTarget
   * @param {!nearbyShare.mojom.TransferMetadata} metadata
   */
  onTransferUpdate(shareTarget, metadata) {
    this.transferStatus_ = metadata.status;

    if (metadata.status ===
        nearbyShare.mojom.TransferStatus.kAwaitingLocalConfirmation) {
      this.shareTarget = shareTarget;
      this.connectionToken =
          (metadata && metadata.token) ? metadata.token : null;
      this.showConfirmPage();
    }
  },

  /**
   * Mojo callback when the Nearby utility process stops.
   */
  onNearbyProcessStopped() {
    this.nearbyProcessStopped_ = true;
  },

  /**
   * Mojo callback when advertising fails to start.
   */
  onStartAdvertisingFailure() {
    this.startAdvertisingFailed_ = true;
  },

  /**
   * @param {PolymerDeepPropertyChange} change a change record
   * @private
   */
  onSettingsChanged_(change) {
    if (change.path !== 'settings.enabled') {
      return;
    }

    if (this.postSettingsCallback) {
      this.postSettingsCallback();
      this.postSettingsCallback = null;
    }
  },

  /**
   * @return {!CrViewManagerElement} the view manager
   * @private
   */
  getViewManager_() {
    return /** @type {!CrViewManagerElement} */ (this.$.viewManager);
  },

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
  },

  /**
   * Defers running a callback for page navigation in the case that we do not
   * yet have a settings.enabled value from mojo or if Nearby Share is not
   * enabled yet and we need to run the onboarding flow first.
   * @param {function()} callback
   * @return {boolean} true if the callback has been scheduled for later, false
   *     if it did not need to be deferred and can be called now.
   */
  deferCallIfNecessary(callback) {
    const haveSettings = !this.settings || this.settings.enabled === undefined;
    if (haveSettings) {
      // Let onSettingsChanged_ handle the navigation because we don't know yet
      // if the feature is enabled and we might need to show onboarding.
      this.postSettingsCallback = callback;
      return true;
    }

    if (!this.settings.enabled) {
      // We need to show onboarding first because nearby is not enabled, but we
      // need to run the callback post onboarding.
      this.postOnboardingCallback = callback;
      this.getViewManager_().switchView(Page.ONBOARDING);
      return true;
    }
    // We know the feature is enabled so no need to defer the call.
    return false;
  },

  /**
   * Call to show the onboarding flow and then close when complete.
   */
  showOnboarding() {
    // Setup the callback to close this dialog when onboarding is complete.
    this.postOnboardingCallback = this.close_.bind(this);
    this.getViewManager_().switchView(Page.ONBOARDING);
  },

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
  },

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
  },

  /**
   * Child views can fire a 'change-page' event to trigger a page change.
   * @param {!CustomEvent<!{page: Page}>} event
   * @private
   */
  onChangePage_(event) {
    this.getViewManager_().switchView(event.detail.page);
  },

  /** @private */
  onCancel_() {
    this.close_();
  },

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
  },

  /** @private */
  onOnboardingComplete_() {
    if (!this.postOnboardingCallback) {
      return;
    }

    this.postOnboardingCallback();
    this.postOnboardingCallback = null;
  },

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
  },
});
