// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
'use strict';

/**
 * The duration in ms of a background flash when a user touches the fingerprint
 * sensor on this page.
 * @type {number}
 */
const FLASH_DURATION_MS = 500;

Polymer({
  is: 'settings-fingerprint-list',

  behaviors: [
    I18nBehavior,
    WebUIListenerBehavior,
    settings.RouteObserverBehavior,
  ],

  properties: {
    /**
     * Authentication token provided by settings-people-page.
     */
    authToken: {
      type: String,
      value: '',
    },

    /**
     * The list of fingerprint objects.
     * @private {!Array<string>}
     */
    fingerprints_: {
      type: Array,
      value: function() {
        return [];
      }
    },

    /** @private */
    showSetupFingerprintDialog_: Boolean,

    /**
     * Whether add another finger is allowed.
     * @type {boolean}
     * @private
     */
    allowAddAnotherFinger_: {
      type: Boolean,
      value: true,
    },
  },

  /** @private {?settings.FingerprintBrowserProxy} */
  browserProxy_: null,

  /** @override */
  attached: function() {
    this.addWebUIListener(
        'on-fingerprint-attempt-received', this.onAttemptReceived_.bind(this));
    this.addWebUIListener('on-screen-locked', this.onScreenLocked_.bind(this));
    this.browserProxy_ = settings.FingerprintBrowserProxyImpl.getInstance();
    this.browserProxy_.startAuthentication();
    this.updateFingerprintsList_();
  },

  /** @override */
  detached: function() {
    this.browserProxy_.endCurrentAuthentication();
  },

  /**
   * Overridden from settings.RouteObserverBehavior.
   * @param {!settings.Route} newRoute
   * @param {!settings.Route} oldRoute
   * @protected
   */
  currentRouteChanged: function(newRoute, oldRoute) {
    if (newRoute != settings.routes.FINGERPRINT) {
      if (this.browserProxy_) {
        this.browserProxy_.endCurrentAuthentication();
      }
    } else if (oldRoute == settings.routes.LOCK_SCREEN) {
      // Start fingerprint authentication when going from LOCK_SCREEN to
      // FINGERPRINT page.
      this.browserProxy_.startAuthentication();
    }
  },

  /**
   * Sends a ripple when the user taps the sensor with a registered fingerprint.
   * @param {!settings.FingerprintAttempt} fingerprintAttempt
   * @private
   */
  onAttemptReceived_: function(fingerprintAttempt) {
    /** @type {NodeList<!HTMLElement>} */ const listItems =
        this.$.fingerprintsList.querySelectorAll('.list-item');
    /** @type {Array<number>} */ const filteredIndexes =
        fingerprintAttempt.indexes.filter(function(index) {
          return index >= 0 && index < listItems.length;
        });

    // Flash the background and produce a ripple for each list item that
    // corresponds to the attempted finger.
    filteredIndexes.forEach(function(index) {
      const listItem = listItems[index];
      const ripple = listItem.querySelector('paper-ripple');

      // Activate the ripple.
      if (ripple) {
        ripple.simulatedRipple();
      }

      // Flash the background.
      listItem.animate(
          [
            {backgroundColor: ['var(--google-grey-300)']},
            {backgroundColor: ['white']}
          ],
          FLASH_DURATION_MS);
    });
  },

  /** @private */
  updateFingerprintsList_: function() {
    this.browserProxy_.getFingerprintsList().then(
        this.onFingerprintsChanged_.bind(this));
  },

  /**
   * @param {!settings.FingerprintInfo} fingerprintInfo
   * @private
   */
  onFingerprintsChanged_: function(fingerprintInfo) {
    // Update iron-list.
    this.fingerprints_ = fingerprintInfo.fingerprintsList.slice();
    this.$$('.action-button').disabled = fingerprintInfo.isMaxed;
    this.allowAddAnotherFinger_ = !fingerprintInfo.isMaxed;
  },

  /**
   * Deletes a fingerprint from |fingerprints_|.
   * @param {!{model: !{index: !number}}} e
   * @private
   */
  onFingerprintDeleteTapped_: function(e) {
    this.browserProxy_.removeEnrollment(e.model.index).then(success => {
      if (success) {
        this.updateFingerprintsList_();
      }
    });
  },

  /**
   * @param {!{model: !{index: !number, item: !string}}} e
   * @private
   */
  onFingerprintLabelChanged_: function(e) {
    this.browserProxy_.changeEnrollmentLabel(e.model.index, e.model.item)
        .then(success => {
          if (success) {
            this.updateFingerprintsList_();
          }
        });
  },

  /**
   * Opens the setup fingerprint dialog.
   * @private
   */
  openAddFingerprintDialog_: function() {
    this.showSetupFingerprintDialog_ = true;
  },

  /** @private */
  onSetupFingerprintDialogClose_: function() {
    this.showSetupFingerprintDialog_ = false;
    cr.ui.focusWithoutInk(assert(this.$$('#addFingerprint')));
    this.browserProxy_.startAuthentication();
  },

  /**
   * Close the setup fingerprint dialog when the screen is unlocked.
   * @param {boolean} screenIsLocked
   * @private
   */
  onScreenLocked_: function(screenIsLocked) {
    if (!screenIsLocked &&
        settings.getCurrentRoute() == settings.routes.FINGERPRINT) {
      this.onSetupFingerprintDialogClose_();
    }
  },

  /**
   * @param {string} item
   * @return {string}
   * @private
   */
  getButtonAriaLabel_: function(item) {
    return this.i18n('lockScreenDeleteFingerprintLabel', item);
  },
});
})();
