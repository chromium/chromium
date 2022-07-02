// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Screens that are used throughout ChromeOS onboarding experience.
// Please keep the sections alphabetically sorted.

// COMMON SCREENS
import './screens/common/adb_sideloading.m.js';
import './screens/common/app_downloading.m.js';
import './screens/common/app_launch_splash.m.js';
import './screens/common/arc_terms_of_service.m.js';
import './screens/common/assistant_optin.m.js';
import './screens/common/autolaunch.m.js';
import './screens/common/consolidated_consent.m.js';
import './screens/common/device_disabled.m.js';
import './screens/common/enable_kiosk.m.js';
import './screens/common/error_message.m.js';
import './screens/common/family_link_notice.m.js';
import './screens/common/fingerprint_setup.m.js';
import './screens/common/gaia_signin.m.js';
import './screens/common/gesture_navigation.m.js';
import './screens/common/guest_tos.m.js';
import './screens/common/hw_data_collection.m.js';
import './screens/common/managed_terms_of_service.m.js';
import './screens/common/marketing_opt_in.m.js';
import './screens/common/multidevice_setup.m.js';
import './screens/common/offline_ad_login.m.js';
import './screens/common/oobe_eula.m.js';
import './screens/common/oobe_reset.m.js';
import './screens/common/os_install.m.js';
import './screens/common/os_trial.m.js';
import './screens/common/parental_handoff.m.js';
import './screens/common/pin_setup.m.js';
import './screens/common/recommend_apps.m.js';
import './screens/common/saml_confirm_password.m.js';
import './screens/common/signin_fatal_error.m.js';
import './screens/common/smart_privacy_protection.m.js';
import './screens/common/sync_consent.m.js';
import './screens/common/theme_selection.m.js';
import './screens/common/tpm_error.m.js';
import './screens/common/user_creation.m.js';
import './screens/common/wrong_hwid.m.js';
// SCREENS USED DURING THE LOGIN FLOW
import './screens/login/active_directory_password_change.m.js';
import './screens/login/encryption_migration.m.js';
import './screens/login/gaia_password_changed.m.js';
import './screens/login/lacros_data_migration.m.js';
import './screens/login/management_transition.m.js';
import './screens/login/offline_login.m.js';
import './screens/login/update_required_card.m.js';
// SCREENS USED DURING THE OOBE FLOW
import './screens/oobe/auto_enrollment_check.m.js';
import './screens/oobe/demo_preferences.m.js';
import './screens/oobe/demo_setup.m.js';
import './screens/oobe/enable_debugging.m.js';
import './screens/oobe/enterprise_enrollment.m.js';
import './screens/oobe/hid_detection.m.js';
import './screens/oobe/oobe_network.m.js';
import './screens/oobe/packaged_license.m.js';
import './screens/oobe/quick_start.m.js';
import './screens/oobe/update.m.js';
import './screens/oobe/welcome.m.js';

/**
 * List of screens that are used for both `oobe` and `login` flows.
 */
export const commonScreensList = [
   {tag: 'adb-sideloading-element', id: 'adb-sideloading'},
   {tag: 'app-downloading-element', id: 'app-downloading'},
   {tag: 'app-launch-splash-element', id: 'app-launch-splash'},
   {tag: 'arc-tos-element', id: 'arc-tos', extra_classes: ['right', 'arc-tos-loading']},
   {tag: 'assistant-optin-element', id: 'assistant-optin-flow'},
   {tag: 'autolaunch-element', id: 'autolaunch'},
   {tag: 'consolidated-consent-element', id: 'consolidated-consent'},
   {tag: 'device-disabled-element', id: 'device-disabled'},
   {tag: 'enable-kiosk-element', id: 'kiosk-enable'},
   {tag: 'error-message-element', id: 'error-message'},
   {tag: 'family-link-notice-element', id: 'family-link-notice'},
   {tag: 'fingerprint-setup-element', id: 'fingerprint-setup'},
   {tag: 'gaia-signin-element', id: 'gaia-signin'},
   {tag: 'gesture-navigation-element', id: 'gesture-navigation'},
   {tag: 'guest-tos-element', id: 'guest-tos'},
   {tag: 'hw-data-collection-element', id: 'hw-data-collection'},
   {tag: 'managed-terms-of-service-element', id: 'terms-of-service'},
   {tag: 'marketing-opt-in-element', id: 'marketing-opt-in'},
   {tag: 'multidevice-setup-element', id: 'multidevice-setup-screen'},
   {tag: 'offline-ad-login-element', id: 'offline-ad-login'},
   {tag: 'oobe-eula-element', id: 'oobe-eula-md'},
   {tag: 'oobe-reset-element', id: 'reset'},
   {tag: 'os-install-element', id: 'os-install', condition: 'isOsInstallAllowed'},
   {tag: 'os-trial-element', id: 'os-trial', condition: 'isOsInstallAllowed'},
   {tag: 'parental-handoff-element', id: 'parental-handoff'},
   {tag: 'pin-setup-element', id: 'pin-setup'},
   {tag: 'recommend-apps-element', id: 'recommend-apps'},
   {tag: 'saml-confirm-password-element', id: 'saml-confirm-password'},
   {tag: 'signin-fatal-error-element', id: 'signin-fatal-error'},
   {tag: 'smart-privacy-protection-element', id: 'smart-privacy-protection'},
   {tag: 'sync-consent-element', id: 'sync-consent'},
   {tag: 'theme-selection-element', id: 'theme-selection'},
   {tag: 'tpm-error-message-element', id: 'tpm-error-message'},
   {tag: 'user-creation-element', id: 'user-creation'},
   {tag: 'wrong-hwid-element', id: 'wrong-hwid'},
];

/**
 * List of screens that are used during the `login` flow only.
 */
export const loginScreensList = [
    {tag: 'active-directory-password-change-element', id: 'ad-password-change'},
    {tag: 'encryption-migration-element', id: 'encryption-migration'},
    {tag: 'gaia-password-changed-element', id: 'gaia-password-changed'},
    {tag: 'lacros-data-migration-element', id: 'lacros-data-migration', extra_classes: ['migrate']},
    {tag: 'management-transition-element', id: 'management-transition', extra_classes: ['migrate']},
    {tag: 'offline-login-element', id: 'offline-login'},
    {tag: 'update-required-card-element', id: 'update-required'},
];

/**
 * List of screens that are used during the `oobe` flow only.
 */
export const oobeScreensList = [
    {tag: 'auto-enrollment-check-element', id: 'auto-enrollment-check'},
    {tag: 'demo-preferences-element', id: 'demo-preferences'},
    {tag: 'demo-setup-element', id: 'demo-setup'},
    {tag: 'enable-debugging-element', id: 'debugging'},
    {tag: 'enterprise-enrollment-element', id: 'enterprise-enrollment'},
    {tag: 'hid-detection-element', id: 'hid-detection'},
    {tag: 'oobe-network-element', id: 'network-selection'},
    {tag: 'packaged-license-element', id: 'packaged-license'},
    {tag: 'quick-start-element', id: 'quick-start'},
    {tag: 'update-element', id: 'oobe-update'},
    {tag: 'oobe-welcome-element', id: 'connect'},
];
