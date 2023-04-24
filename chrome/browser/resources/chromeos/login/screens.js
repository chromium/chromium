// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Screens that are used throughout ChromeOS onboarding experience.
// Please keep the sections alphabetically sorted.

// COMMON SCREENS
import './screens/common/adb_sideloading.js';
import './screens/common/app_downloading.js';
import './screens/common/app_launch_splash.js';
import './screens/common/assistant_optin.js';
import './screens/common/autolaunch.js';
import './screens/common/choobe.js';
import './screens/common/consolidated_consent.js';
import './screens/common/cryptohome_recovery_setup.js';
import './screens/common/device_disabled.js';
import './screens/common/display_size.js';
import './screens/common/drive_pinning.js';
import './screens/common/enable_kiosk.js';
import './screens/common/error_message.js';
import './screens/common/family_link_notice.js';
import './screens/common/fingerprint_setup.js';
import './screens/common/gaia_signin.js';
import './screens/common/gesture_navigation.js';
import './screens/common/guest_tos.js';
import './screens/common/hw_data_collection.js';
import './screens/common/local_state_error.js';
import './screens/common/managed_terms_of_service.js';
import './screens/common/marketing_opt_in.js';
import './screens/common/multidevice_setup.js';
import './screens/common/offline_ad_login.js';
import './screens/common/oobe_reset.js';
import './screens/common/os_install.js';
import './screens/common/os_trial.js';
import './screens/common/parental_handoff.js';
import './screens/common/pin_setup.js';
import './screens/common/recommend_apps.js';
import './screens/common/saml_confirm_password.js';
import './screens/common/signin_fatal_error.js';
import './screens/common/smart_privacy_protection.js';
import './screens/common/sync_consent.js';
import './screens/common/theme_selection.js';
import './screens/common/touchpad_scroll.js';
import './screens/common/tpm_error.js';
import './screens/common/user_creation.js';
import './screens/common/wrong_hwid.js';
// SCREENS USED DURING THE LOGIN FLOW
import './screens/login/active_directory_password_change.js';
import './screens/login/arc_vm_data_migration.js';
import './screens/login/cryptohome_recovery.js';
import './screens/login/encryption_migration.js';
import './screens/login/gaia_password_changed.js';
import './screens/login/lacros_data_backward_migration.js';
import './screens/login/lacros_data_migration.js';
import './screens/login/management_transition.js';
import './screens/login/offline_login.js';
import './screens/login/update_required_card.js';
// SCREENS USED DURING THE OOBE FLOW
import './screens/oobe/auto_enrollment_check.js';
import './screens/oobe/demo_preferences.js';
import './screens/oobe/demo_setup.js';
import './screens/oobe/enable_debugging.js';
import './screens/oobe/enterprise_enrollment.js';
import './screens/oobe/hid_detection.js';
import './screens/oobe/oobe_network.js';
import './screens/oobe/packaged_license.js';
import './screens/oobe/quick_start.js';
import './screens/oobe/update.js';
import './screens/oobe/welcome.js';

/**
 * List of screens that are used for both `oobe` and `login` flows.
 */
export const commonScreensList = [
  {tag: 'adb-sideloading-element', id: 'adb-sideloading'},
  {tag: 'app-downloading-element', id: 'app-downloading'},
  {tag: 'app-launch-splash-element', id: 'app-launch-splash'},
  {tag: 'assistant-optin-element', id: 'assistant-optin-flow'},
  {tag: 'autolaunch-element', id: 'autolaunch'},
  {
    tag: 'choobe-element',
    id: 'choobe',
    condition: 'isChoobeEnabled',
  },
  {tag: 'consolidated-consent-element', id: 'consolidated-consent'},
  {tag: 'cryptohome-recovery-setup-element', id: 'cryptohome-recovery-setup'},
  {tag: 'device-disabled-element', id: 'device-disabled'},
  {
    tag: 'display-size-element',
    id: 'display-size',
    condition: 'isDisplaySizeEnabled',
  },
  {

    tag: 'drive-pinning-element',
    id: 'drive-pinning',
    condition: 'isDrivePinningEnabled',
  },
  {tag: 'enable-kiosk-element', id: 'kiosk-enable'},
  {tag: 'error-message-element', id: 'error-message'},
  {tag: 'family-link-notice-element', id: 'family-link-notice'},
  {tag: 'fingerprint-setup-element', id: 'fingerprint-setup'},
  {tag: 'gaia-signin-element', id: 'gaia-signin'},
  {tag: 'gesture-navigation-element', id: 'gesture-navigation'},
  {tag: 'guest-tos-element', id: 'guest-tos'},
  {tag: 'hw-data-collection-element', id: 'hw-data-collection'},
  {tag: 'local-state-error-element', id: 'local-state-error'},
  {tag: 'managed-terms-of-service-element', id: 'terms-of-service'},
  {tag: 'marketing-opt-in-element', id: 'marketing-opt-in'},
  {tag: 'multidevice-setup-element', id: 'multidevice-setup-screen'},
  {tag: 'offline-ad-login-element', id: 'offline-ad-login'},
  {tag: 'oobe-reset-element', id: 'reset'},
  {
    tag: 'os-install-element',
    id: 'os-install',
    condition: 'isOsInstallAllowed',
  },
  {tag: 'os-trial-element', id: 'os-trial', condition: 'isOsInstallAllowed'},
  {tag: 'parental-handoff-element', id: 'parental-handoff'},
  {tag: 'pin-setup-element', id: 'pin-setup'},
  {tag: 'recommend-apps-element', id: 'recommend-apps'},
  {tag: 'saml-confirm-password-element', id: 'saml-confirm-password'},
  {tag: 'signin-fatal-error-element', id: 'signin-fatal-error'},
  {tag: 'smart-privacy-protection-element', id: 'smart-privacy-protection'},
  {tag: 'sync-consent-element', id: 'sync-consent'},
  {tag: 'theme-selection-element', id: 'theme-selection'},
  {
    tag: 'touchpad-scroll-element',
    id: 'touchpad-scroll',
    condition: 'isTouchpadScrollEnabled',
  },
  {tag: 'tpm-error-message-element', id: 'tpm-error-message'},
  {tag: 'user-creation-element', id: 'user-creation'},
  {tag: 'wrong-hwid-element', id: 'wrong-hwid'},
];

/**
 * List of screens that are used during the `login` flow only.
 */
export const loginScreensList = [
  {tag: 'active-directory-password-change-element', id: 'ad-password-change'},
  {
    tag: 'arc-vm-data-migration-element',
    id: 'arc-vm-data-migration',
    condition: 'isArcVmDataMigrationEnabled',
  },
  {tag: 'cryptohome-recovery-element', id: 'cryptohome-recovery'},
  {tag: 'encryption-migration-element', id: 'encryption-migration'},
  {tag: 'gaia-password-changed-element', id: 'gaia-password-changed'},
  {
    tag: 'lacros-data-backward-migration-element',
    id: 'lacros-data-backward-migration',
    extra_classes: ['migrate'],
  },
  {
    tag: 'lacros-data-migration-element',
    id: 'lacros-data-migration',
    extra_classes: ['migrate'],
  },
  {
    tag: 'management-transition-element',
    id: 'management-transition',
    extra_classes: ['migrate'],
  },
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
