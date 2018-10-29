// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Shared data between oobe and the login screens. These files
 * are *not* used in the lock screen. If a file should also be used in the lock
 * screen, added it to login_shared.js.
 */

// <include src="oobe_screen_reset.js">
// <include src="oobe_screen_autolaunch.js">
// <include src="oobe_screen_enable_kiosk.js">
// <include src="oobe_screen_terms_of_service.js">
// <include src="oobe_screen_user_image.js">
// <include src="oobe_screen_voice_interaction_value_prop.js">
// <include src="oobe_screen_wait_for_container_ready.js">
// <include src="oobe_screen_assistant_optin_flow.js">
// <include src="oobe_select.js">

// <include src="screen_app_launch_splash.js">
// <include src="screen_arc_kiosk_splash.js">
// <include src="screen_arc_terms_of_service.js">
// <include src="screen_error_message.js">
// <include src="screen_gaia_signin.js">
// <include src="screen_password_changed.js">
// <include src="screen_tpm_error.js">
// <include src="screen_wrong_hwid.js">
// <include src="screen_confirm_password.js">
// <include src="screen_fatal_error.js">
// <include src="screen_device_disabled.js">
// <include src="screen_unrecoverable_cryptohome_error.js">
// <include src="screen_active_directory_password_change.js">
// <include src="screen_encryption_migration.js">
// <include src="screen_update_required.js">
// <include src="screen_sync_consent.js">
// <include src="screen_fingerprint_setup.js">
// <include src="screen_recommend_apps.js">
// <include src="screen_app_downloading.js">
// <include src="screen_discover.js">
// <include src="screen_marketing_opt_in.js">
// <include src="screen_multidevice_setup.js">

// <include src="../../gaia_auth_host/authenticator.js">

// Register assets for async loading.
[{
  id: SCREEN_OOBE_ENROLLMENT,
  html: [{url: 'chrome://oobe/enrollment.html', targetID: 'inner-container'}],
  css: ['chrome://oobe/enrollment.css'],
  js: ['chrome://oobe/enrollment.js']
}].forEach(cr.ui.login.ResourceLoader.registerAssets);

(function() {
'use strict';

document.addEventListener('DOMContentLoaded', function() {
  // Immediately load async assets.
  cr.ui.login.ResourceLoader.loadAssets(SCREEN_OOBE_ENROLLMENT, function() {
    // This screen is async-loaded so we manually trigger i18n processing.
    i18nTemplate.process($('oauth-enrollment'), loadTimeData);
    // Delayed binding since this isn't defined yet.
    login.OAuthEnrollmentScreen.register();
  });
});
})();
