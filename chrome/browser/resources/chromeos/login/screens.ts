// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Screens that are used throughout ChromeOS onboarding experience.
// Please keep the sections alphabetically sorted.

// COMMON SCREENS
import './screens/common/adb_sideloading.js';
import './screens/common/add_child.js';
import './screens/common/ai_intro.js';
import './screens/common/app_downloading.js';
import './screens/common/app_launch_splash.js';
import './screens/common/assistant_optin.js';
import './screens/common/categories_selection.js';
import './screens/common/choobe.js';
import './screens/common/consolidated_consent.js';
import './screens/common/device_disabled.js';
import './screens/common/display_size.js';
import './screens/common/drive_pinning.js';
import './screens/common/error_message.js';
import './screens/common/family_link_notice.js';
import './screens/common/gaia_info.js';
import './screens/common/gaia_signin.js';
import './screens/common/gemini_intro.js';
import './screens/common/gesture_navigation.js';
import './screens/common/guest_tos.js';
import './screens/common/hw_data_collection.js';
import './screens/common/install_attributes_error.js';
import './screens/common/local_state_error.js';
import './screens/common/managed_terms_of_service.js';
import './screens/common/marketing_opt_in.js';
import './screens/common/multidevice_setup.js';
import './screens/common/online_authentication_screen.js';
import './screens/common/oobe_reset.js';
import './screens/common/os_install.js';
import './screens/common/os_trial.js';
import './screens/common/perks_discovery.js';
import './screens/common/personalized_recommend_apps.js';
import './screens/common/parental_handoff.js';
import './screens/common/quick_start.js';
import './screens/common/recommend_apps.js';
import './screens/common/remote_activity_notification.js';
import './screens/common/saml_confirm_password.js';
import './screens/common/signin_fatal_error.js';
import './screens/common/smart_privacy_protection.js';
import './screens/common/split_modifier_keyboard_info.js';
import './screens/common/sync_consent.js';
import './screens/common/theme_selection.js';
import './screens/common/touchpad_scroll.js';
import './screens/common/tpm_error.js';
import './screens/common/user_allowlist_check_screen.js';
import './screens/common/wrong_hwid.js';
// COMMON SCREENS USED TO SET UP AUTHENTICATION
import './screens/osauth/apply_online_password.js';
import './screens/osauth/cryptohome_recovery_setup.js';
import './screens/osauth/factor_setup_success.js';
import './screens/osauth/fingerprint_setup.js';
import './screens/osauth/local_password_setup.js';
import './screens/osauth/local_data_loss_warning.js';
import './screens/osauth/enter_old_password.js';
import './screens/osauth/osauth_error.js';
import './screens/osauth/password_selection.js';
import './screens/osauth/pin_setup.js';
// AUTHENTICATION SCREENS USED DURING THE LOGIN FLOW
import './screens/osauth/cryptohome_recovery.js';
// SCREENS USED DURING THE LOGIN FLOW
import './screens/login/arc_vm_data_migration.js';
import './screens/login/encryption_migration.js';
import './screens/login/management_transition.js';
import './screens/login/offline_login.js';
import './screens/login/update_required_card.js';
import './screens/common/account_selection.js';
// SCREENS USED DURING THE OOBE FLOW
import './screens/oobe/auto_enrollment_check.js';
import './screens/oobe/consumer_update.js';
import './screens/oobe/demo_preferences.js';
import './screens/oobe/demo_setup.js';
import './screens/oobe/enable_debugging.js';
import './screens/oobe/enterprise_enrollment.js';
import './screens/oobe/hid_detection.js';
import './screens/oobe/oobe_network.js';
import './screens/oobe/packaged_license.js';
import './screens/oobe/update.js';

import {OobeTypes} from './components/oobe_types.js';

/**
 * List of screens that are used for both `oobe` and `login` flows.
 */
export const commonScreensList: OobeTypes.ScreensList = [
  {tag: 'adb-sideloading-element', id: 'adb-sideloading'},
  {tag: 'add-child-element', id: 'add-child'},
  {
    tag: 'ai-intro-element',
    id: 'ai-intro',
    condition: 'isOobeAiIntroEnabled',
  },
  {tag: 'app-downloading-element', id: 'app-downloading'},
  {tag: 'app-launch-splash-element', id: 'app-launch-splash'},
  {
    tag: 'assistant-optin-element',
    id: 'assistant-optin-flow',
    condition: 'isOobeAssistantEnabled',
  },
  {
    tag: 'apply-online-password-element',
    id: 'apply-online-password',
  },
  {
    tag: 'categories-selection-element',
    id: 'categories-selection',
    condition: 'isPersonalizedOnboarding',
  },
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
  {
    tag: 'enter-old-password-element',
    id: 'enter-old-password',
  },
  {tag: 'error-message-element', id: 'error-message'},
  {tag: 'family-link-notice-element', id: 'family-link-notice'},
  {tag: 'fingerprint-setup-element', id: 'fingerprint-setup'},
  {
    tag: 'gaia-info-element',
    id: 'gaia-info',
    condition: 'isOobeGaiaInfoScreenEnabled',
  },
  {tag: 'gaia-signin-element', id: 'gaia-signin'},
  {
    tag: 'gemini-intro-element',
    id: 'gemini-intro',
    condition: 'isOobeGeminiIntroEnabled',
  },
  {tag: 'gesture-navigation-element', id: 'gesture-navigation'},
  {tag: 'guest-tos-element', id: 'guest-tos'},
  {tag: 'hw-data-collection-element', id: 'hw-data-collection'},
  {
    tag: 'local-password-setup-element',
    id: 'local-password-setup',
  },
  {
    tag: 'local-data-loss-warning-element',
    id: 'local-data-loss-warning',
  },
  {tag: 'local-state-error-element', id: 'local-state-error'},
  {tag: 'managed-terms-of-service-element', id: 'terms-of-service'},
  {tag: 'marketing-opt-in-element', id: 'marketing-opt-in'},
  {tag: 'multidevice-setup-element', id: 'multidevice-setup-screen'},
  {
    tag: 'online-authentication-screen-element',
    id: 'online-authentication-screen',
  },
  {tag: 'oobe-reset-element', id: 'reset'},
  {tag: 'osauth-error-element', id: 'osauth-error'},
  {
    tag: 'perks-discovery-element',
    id: 'perks-discovery',
    condition: 'isPerksDiscoveryEnabled',
  },
  {
    tag: 'personalized-apps-element',
    id: 'personalized-apps',
    condition: 'isPersonalizedOnboarding',
  },
  {tag: 'factor-setup-success-element', id: 'factor-setup-success'},
  {
    tag: 'os-install-element',
    id: 'os-install',
    condition: 'isOsInstallAllowed',
  },
  {tag: 'os-trial-element', id: 'os-trial', condition: 'isOsInstallAllowed'},
  {tag: 'parental-handoff-element', id: 'parental-handoff'},
  {
    tag: 'password-selection-element',
    id: 'password-selection',
  },
  {tag: 'pin-setup-element', id: 'pin-setup'},
  {tag: 'quick-start-element', id: 'quick-start'},
  {tag: 'recommend-apps-element', id: 'recommend-apps'},
  {
    tag: 'remote-activity-notification-element',
    id: 'remote-activity-notification',
    condition: 'isRemoteActivityNotificationEnabled',
  },
  {tag: 'saml-confirm-password-element', id: 'saml-confirm-password'},
  {tag: 'signin-fatal-error-element', id: 'signin-fatal-error'},
  {tag: 'smart-privacy-protection-element', id: 'smart-privacy-protection'},
  {
    tag: 'split-modifier-keyboard-info-element',
    id: 'split-modifier-keyboard-info',
    condition: 'isSplitModifierKeyboardInfoEnabled',
  },
  {tag: 'sync-consent-element', id: 'sync-consent'},
  {tag: 'theme-selection-element', id: 'theme-selection'},
  {
    tag: 'touchpad-scroll-element',
    id: 'touchpad-scroll',
    condition: 'isTouchpadScrollEnabled',
  },
  {tag: 'tpm-error-message-element', id: 'tpm-error-message'},
  {
    tag: 'install-attributes-error-message-element',
    id: 'install-attributes-error-message',
  },
  {
    tag: 'user-allowlist-check-screen-element',
    id: 'user-allowlist-check-screen',
  },
  {tag: 'wrong-hwid-element', id: 'wrong-hwid'},
];

/**
 * List of screens that are used during the `login` flow only.
 */
export const loginScreensList: OobeTypes.ScreensList = [
  {
    tag: 'arc-vm-data-migration-element',
    id: 'arc-vm-data-migration',
    condition: 'isArcVmDataMigrationEnabled',
  },
  {tag: 'cryptohome-recovery-element', id: 'cryptohome-recovery'},
  {tag: 'encryption-migration-element', id: 'encryption-migration'},
  {
    tag: 'management-transition-element',
    id: 'management-transition',
    extra_classes: ['migrate'],
  },
  {tag: 'offline-login-element', id: 'offline-login'},
  {tag: 'update-required-card-element', id: 'update-required'},
  {
    tag: 'account-selection-element',
    id: 'account-selection',
    condition: 'isOobeAddUserDuringEnrollmentEnabled',
  },
];

/**
 * List of screens that are used during the `oobe` flow only.
 */
export const oobeScreensList: OobeTypes.ScreensList = [
  {tag: 'auto-enrollment-check-element', id: 'auto-enrollment-check'},
  {
    tag: 'consumer-update-element',
    id: 'consumer-update',
    condition: 'isSoftwareUpdateEnabled',
  },
  {tag: 'demo-preferences-element', id: 'demo-preferences'},
  {tag: 'demo-setup-element', id: 'demo-setup'},
  {tag: 'enable-debugging-element', id: 'debugging'},
  {tag: 'enterprise-enrollment-element', id: 'enterprise-enrollment'},
  {tag: 'hid-detection-element', id: 'hid-detection'},
  {tag: 'oobe-network-element', id: 'network-selection'},
  {tag: 'packaged-license-element', id: 'packaged-license'},
  {tag: 'update-element', id: 'oobe-update'},
];
