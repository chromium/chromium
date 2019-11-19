// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

login.createScreen('OAuthEnrollmentScreen', 'oauth-enrollment', function() {
  return {
    EXTERNAL_API: [
      'showStep',
      'showError',
      'doReload',
      'setAvailableLicenseTypes',
      'showAttributePromptStep',
      'showAttestationBasedEnrollmentSuccess',
      'setAdJoinParams',
      'setAdJoinConfiguration',
    ],

    /**
     * Returns the control which should receive initial focus.
     */
    get defaultControl() {
      return $('enterprise-enrollment');
    },

    /**
     * This is called after resources are updated.
     */
    updateLocalizedContent: function() {
      $('enterprise-enrollment').updateLocalizedContent();
    },


    /** @override */
    decorate: function() {
      $('enterprise-enrollment').screen = this;
    },

    /**
     * Shows attribute-prompt step with pre-filled asset ID and
     * location.
     */
    showAttributePromptStep: function(annotatedAssetId, annotatedLocation) {
      $('enterprise-enrollment')
          .showAttributePromptStep(annotatedAssetId, annotatedLocation);
    },

    /**
     * Shows a success card for attestation-based enrollment that shows
     * which domain the device was enrolled into.
     */
    showAttestationBasedEnrollmentSuccess: function(
        device, enterpriseEnrollmentDomain) {
      $('enterprise-enrollment')
          .showAttestationBasedEnrollmentSuccess(
              device, enterpriseEnrollmentDomain);
    },

    /**
     * Updates the list of available license types in license selection dialog.
     */
    setAvailableLicenseTypes: function(licenseTypes) {
      $('enterprise-enrollment').setAvailableLicenseTypes(licenseTypes);
    },

    /**
     * Switches between the different steps in the enrollment flow.
     * @param {string} step the steps to show, one of "signin", "working",
     * "attribute-prompt", "error", "success".
     */
    showStep: function(step) {
      $('enterprise-enrollment').showStep(step);
    },

    /**
     * Sets an error message and switches to the error screen.
     * @param {string} message the error message.
     * @param {boolean} retry whether the retry link should be shown.
     */
    showError: function(message, retry) {
      $('enterprise-enrollment').showError(message, retry);
    },

    doReload: function() {
      $('enterprise-enrollment').doReload();
    },

    /**
     * Sets Active Directory join screen params.
     * @param {string} machineName
     * @param {string} userName
     * @param {ACTIVE_DIRECTORY_ERROR_STATE} errorState
     * @param {boolean} showUnlockConfig true if there is an encrypted
     * configuration (and not unlocked yet).
     */
    setAdJoinParams: function(
        machineName, userName, errorState, showUnlockConfig) {
      $('enterprise-enrollment')
          .setAdJoinParams(machineName, userName, errorState, showUnlockConfig);
    },

    /**
     * Sets Active Directory join screen with the unlocked configuration.
     * @param {Array<JoinConfigType>} options
     */
    setAdJoinConfiguration: function(options) {
      $('enterprise-enrollment').setAdJoinConfiguration(options);
    },

    closeEnrollment_: function(result) {
      chrome.send('oauthEnrollClose', [result]);
    },

    onAttributesEntered_: function(asset_id, location) {
      chrome.send('oauthEnrollAttributes', [asset_id, location]);
    },

    onAuthCompleted_: function(email) {
      chrome.send('oauthEnrollCompleteLogin', [email]);
    },

    onAuthFrameLoaded_: function() {
      chrome.send('frameLoadingCompleted');
    },

    onAdCompleteLogin_: function(
        machine_name, distinguished_name, encryption_types, username,
        password) {
      chrome.send('oauthEnrollAdCompleteLogin', [
        machine_name, distinguished_name, encryption_types, username, password
      ]);
    },

    onAdUnlockConfiguration_: function(unlock_password) {
      chrome.send('oauthEnrollAdUnlockConfiguration', [unlock_password]);
    },

    onLicenseTypeSelected_: function(license_type) {
      chrome.send('onLicenseTypeSelected', [license_type]);
    },
  };
});
