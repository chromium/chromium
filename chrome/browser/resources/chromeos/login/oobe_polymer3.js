// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {Oobe} from './cr_ui.m.js';
import * as OobeDebugger from './debug/debug.m.js';
import {invokePolymerMethod} from './display_manager.m.js';
import {loadTimeData} from './i18n_setup.js';
import 'chrome://oobe/components/test_util.m.js';
import 'chrome://oobe/test_api/test_api.m.js';
import {i18nTemplate} from 'chrome://resources/js/i18n_template_no_process.m.js';
// clang-format on

import 'chrome://oobe/screens/common/app_downloading.m.js';
import 'chrome://oobe/screens/common/app_launch_splash.m.js';
import 'chrome://oobe/screens/common/adb_sideloading.m.js';
import 'chrome://oobe/screens/common/arc_terms_of_service.m.js';
import 'chrome://oobe/screens/common/assistant_optin.m.js';
import 'chrome://oobe/screens/common/autolaunch.m.js';
import 'chrome://oobe/screens/common/consolidated_consent.m.js';
import 'chrome://oobe/screens/common/device_disabled.m.js';
import 'chrome://oobe/screens/common/enable_kiosk.m.js';
import 'chrome://oobe/screens/common/error_message.m.js';
import 'chrome://oobe/screens/common/family_link_notice.m.js';
import 'chrome://oobe/screens/common/fingerprint_setup.m.js';
import 'chrome://oobe/screens/common/gaia_signin.m.js';
import 'chrome://oobe/screens/common/gesture_navigation.m.js';
import 'chrome://oobe/screens/common/guest_tos.m.js';
import 'chrome://oobe/screens/common/hw_data_collection.m.js';
import 'chrome://oobe/screens/common/managed_terms_of_service.m.js';
import 'chrome://oobe/screens/common/marketing_opt_in.m.js';
import 'chrome://oobe/screens/common/multidevice_setup.m.js';
import 'chrome://oobe/screens/common/offline_ad_login.m.js';
import 'chrome://oobe/screens/common/oobe_eula.m.js';
import 'chrome://oobe/screens/common/oobe_reset.m.js';
import 'chrome://oobe/screens/common/os_install.m.js';
import 'chrome://oobe/screens/common/os_trial.m.js';
import 'chrome://oobe/screens/common/parental_handoff.m.js';
import 'chrome://oobe/screens/common/pin_setup.m.js';
// TODO(crbug.com/1261902): Remove.
import 'chrome://oobe/screens/common/recommend_apps_old.m.js';
import 'chrome://oobe/screens/common/recommend_apps.m.js';
import 'chrome://oobe/screens/common/saml_confirm_password.m.js';
import 'chrome://oobe/screens/common/signin_fatal_error.m.js';
import 'chrome://oobe/screens/common/smart_privacy_protection.m.js';
import 'chrome://oobe/screens/common/sync_consent.m.js';
import 'chrome://oobe/screens/common/theme_selection.m.js';
import 'chrome://oobe/screens/common/tpm_error.m.js';
import 'chrome://oobe/screens/common/user_creation.m.js';
import 'chrome://oobe/screens/common/wrong_hwid.m.js';
import 'chrome://oobe/screens/login/active_directory_password_change.m.js';
import 'chrome://oobe/screens/login/encryption_migration.m.js';
import 'chrome://oobe/screens/login/gaia_password_changed.m.js';
import 'chrome://oobe/screens/login/lacros_data_migration.m.js';
import 'chrome://oobe/screens/login/management_transition.m.js';
import 'chrome://oobe/screens/login/offline_login.m.js';
import 'chrome://oobe/screens/login/update_required_card.m.js';
import 'chrome://oobe/screens/oobe/auto_enrollment_check.m.js';
import 'chrome://oobe/screens/oobe/demo_preferences.m.js';
import 'chrome://oobe/screens/oobe/demo_setup.m.js';
import 'chrome://oobe/screens/oobe/enable_debugging.m.js';
import 'chrome://oobe/screens/oobe/enterprise_enrollment.m.js';
import 'chrome://oobe/screens/oobe/hid_detection.m.js';
import 'chrome://oobe/screens/oobe/oobe_network.m.js';
import 'chrome://oobe/screens/oobe/welcome.m.js';
import 'chrome://oobe/screens/oobe/packaged_license.m.js';
import 'chrome://oobe/screens/oobe/update.m.js';

function initializeDebugger() {
  if (document.readyState === 'loading')
    return;
  document.removeEventListener('DOMContentLoaded', initializeDebugger);
  OobeDebugger.DebuggerUI.getInstance().register(document.body);
}

// Create the global values attached to `window` that are used
// for accessing OOBE controls from the browser side.
function prepareGlobalValues(globalValue) {
  if (globalValue.cr == undefined) {
    globalValue.cr = {};
  }
  if (globalValue.cr.ui == undefined) {
    globalValue.cr.ui = {};
  }
  if (globalValue.cr.ui.login == undefined) {
    globalValue.cr.ui.login = {};
  }

  // Expose some values in the global object that are needed by OOBE.
  globalValue.cr.ui.Oobe = Oobe;
  globalValue.Oobe = Oobe;
}

(function (root) {
    i18nTemplate.process(document, loadTimeData);
    prepareGlobalValues(window);
    Oobe.initialize();

    // Initialize the debugger if it has been defined.
    if (OobeDebugger.DebuggerUI) {
      if (document.readyState === 'loading') {
          document.addEventListener('DOMContentLoaded', initializeDebugger);
        } else {
          initializeDebugger();
      }
    }
})(window);
