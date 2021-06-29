// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Out of the box experience flow (OOBE).
 * This is the main code for the OOBE WebUI implementation.
 */

// <include src="test_util.js">
// <include src="display_manager.js">
// <include src="components/display_manager_types.js">
// <include src="demo_mode_test_helper.js">

// <include src="login_ui_tools.js">
// <include src="cr_ui.js">
// <include src="components/oobe_select.js">

// <include src="../../gaia_auth_host/authenticator.js">
// <include src="multi_tap_detector.js">
// <include src="web_view_helper.js">

HTMLImports.whenReady(() => {
  i18nTemplate.process(document, loadTimeData);

  cr.define('cr.ui.Oobe', function() {
    return {
      /**
       * Initializes the OOBE flow.  This will cause all C++ handlers to
       * be invoked to do final setup.
       */
      initialize() {
        cr.ui.login.DisplayManager.initialize();

        chrome.send('screenStateInitialize');
      },

      /**
       * Reloads content of the page (localized strings, options of the select
       * controls).
       * @param {!Object} data New dictionary with i18n values.
       */
      reloadContent(data) {
        // Reload global local strings, process DOM tree again.
        loadTimeData.overrideValues(data);
        i18nTemplate.process(document, loadTimeData);

        // Update localized content of the screens.
        Oobe.getInstance().updateLocalizedContent_();
      },

      /**
       * Updates "device in tablet mode" state when tablet mode is changed.
       * @param {Boolean} isInTabletMode True when in tablet mode.
       */
      setTabletModeState(isInTabletMode) {
        Oobe.getInstance().setTabletModeState_(isInTabletMode);
      },

      /**
       * Updates OOBE configuration when it is loaded.
       * @param {!OobeTypes.OobeConfiguration} configuration OOBE configuration.
       */
      updateOobeConfiguration(configuration) {
        Oobe.getInstance().updateOobeConfiguration_(configuration);
      },
    };
  });
  // <include src="oobe_initialization.js">
});
