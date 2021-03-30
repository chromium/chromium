// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Common OOBE controller methods for use in OOBE and login.
 * This file is shared between OOBE and login. Add only methods that need to be
 * shared between all *two* screens here.
 */

cr.define('cr.ui', function() {
  var DisplayManager = cr.ui.login.DisplayManager;

  /**
   * Constructs an Out of box controller. It manages initialization of screens,
   * transitions, error messages display.
   * @extends {DisplayManager}
   * @constructor
   */
  function Oobe() {}

  /**
   * Delay in milliseconds between start of OOBE animation and start of
   * header bar animation.
   */
  var HEADER_BAR_DELAY_MS = 300;

  cr.addSingletonGetter(Oobe);

  Oobe.prototype = {
    __proto__: DisplayManager.prototype,
  };

  /**
   * OOBE initialization coordination. Used by tests to wait for OOBE
   * to fully load when using the HTLImports polyfill.
   * TODO(crbug.com/1111387) - Remove once migrated to JS modules.
   * Remove spammy logging when closer to M89 branch point.
   */
  Oobe.initializationComplete = false;
  Oobe.initCallbacks = [];
  Oobe.waitForOobeToLoad = function() {
    return new Promise(function(resolve, reject) {
      if (cr.ui.Oobe.initializationComplete) {
        // TODO(crbug.com/1111387) - Remove excessive logging.
        console.warn('OOBE is already initialized. Continuing...');
        resolve();
      } else {
        // TODO(crbug.com/1111387) - Remove excessive logging.
        console.warn('OOBE not loaded yet. Waiting...');
        cr.ui.Oobe.initCallbacks.push(resolve);
      }
    });
  };

  /**
   * Called when focus is returned from ash::SystemTray.
   * @param {boolean} reverse Is focus returned in reverse order?
   */
  Oobe.focusReturned = function(reverse) {
    if (Oobe.getInstance().currentScreen &&
        Oobe.getInstance().currentScreen.onFocusReturned) {
      Oobe.getInstance().currentScreen.onFocusReturned(reverse);
    }
  };

  /**
   * Handle accelerators. These are passed from native code instead of a JS
   * event handler in order to make sure that embedded iframes cannot swallow
   * them.
   * @param {string} name Accelerator name.
   */
  Oobe.handleAccelerator = function(name) {
    Oobe.getInstance().handleAccelerator(name);
  };

  /**
   * Shows the given screen.
   * @param {Object} screen Screen params dict, e.g. {id: screenId, data: data}
   */
  Oobe.showScreen = function(screen) {
    Oobe.getInstance().showScreen(screen);
  };

  /**
   * Updates missin API keys message visibility.
   * @param {boolean} show True if the message should be visible.
   */
  Oobe.showAPIKeysNotice = function(show) {
    $('api-keys-notice-container').hidden = !show;
  };

  /**
   * Updates version label visibility.
   * @param {boolean} show True if version label should be visible.
   */
  Oobe.showVersion = function(show) {
    Oobe.getInstance().showVersion(show);
  };

  /**
   * Update body class to switch between OOBE UI and Login UI.
   */
  Oobe.showOobeUI = function(showOobe) {
    if (showOobe) {
      document.body.classList.add('oobe-display');
    } else {
      document.body.classList.remove('oobe-display');
      Oobe.getInstance().prepareForLoginDisplay_();
    }
  };

  /**
   * Enables keyboard driven flow.
   */
  Oobe.enableKeyboardFlow = function(value) {
    // Don't show header bar for OOBE.
    Oobe.getInstance().forceKeyboardFlow = value;
  };

  /**
   * Shows signin UI.
   * @param {string} opt_email An optional email for signin UI.
   */
  Oobe.showSigninUI = function(opt_email) {
    DisplayManager.showSigninUI(opt_email);
  };

  /**
   * Resets sign-in input fields.
   * @param {boolean} forceOnline Whether online sign-in should be forced.
   * If |forceOnline| is false previously used sign-in type will be used.
   */
  Oobe.resetSigninUI = function(forceOnline) {
    DisplayManager.resetSigninUI(forceOnline);
  };

  /**
   * Shows sign-in error bubble.
   * @param {number} loginAttempts Number of login attemps tried.
   * @param {string} message Error message to show.
   * @param {string} link Text to use for help link.
   * @param {number} helpId Help topic Id associated with help link.
   */
  Oobe.showSignInError = function(loginAttempts, message, link, helpId) {
    DisplayManager.showSignInError(loginAttempts, message, link, helpId);
  };

  /**
   * Show user-pods.
   */
  Oobe.showUserPods = function() {
    if (Oobe.getInstance().showingViewsLogin) {
      chrome.send('hideOobeDialog');
      return;
    }
    Oobe.showSigninUI();
    Oobe.resetSigninUI(true);
  };

  /**
   * Clears error bubble.
   */
  Oobe.clearErrors = function() {
    DisplayManager.clearErrors();
  };

  /**
   * Sets text content for a div with |labelId|.
   * @param {string} labelId Id of the label div.
   * @param {string} labelText Text for the label.
   */
  Oobe.setLabelText = function(labelId, labelText) {
    DisplayManager.setLabelText(labelId, labelText);
  };

  /**
   * Sets the text content of the enterprise info message.
   * If the text is empty, the entire notification will be hidden.
   * @param {string} messageText The message text.
   */
  Oobe.setEnterpriseInfo = function(messageText, assetId) {
    DisplayManager.setEnterpriseInfo(messageText, assetId);
  };

  /**
   * Sets the text content of the Bluetooth device info message.
   * @param {string} bluetoothName The Bluetooth device name text.
   */
  Oobe.setBluetoothDeviceInfo = function(bluetoothName) {
    DisplayManager.setBluetoothDeviceInfo(bluetoothName);
  };

  /**
   * Some ForTesting APIs directly access to DOM. Because this script is loaded
   * in header, DOM tree may not be available at beginning.
   * In DOMContentLoaded, after Oobe.initialize() is done, this is marked to
   * true, indicating ForTesting methods can be called.
   * External script using ForTesting APIs should wait for this condition.
   * @type {boolean}
   */
  Oobe.readyForTesting = false;

  /**
   * Skip to login screen for telemetry.
   */
  Oobe.skipToLoginForTesting = function() {
    chrome.send('skipToLoginForTesting');
  };

  /**
   * Skip to update screen for telemetry.
   */
  Oobe.skipToUpdateForTesting = function() {
    chrome.send('skipToUpdateForTesting');
  };

  /**
   * Login for telemetry.
   * @param {string} username Login username.
   * @param {string} password Login password.
   * @param {boolean} enterpriseEnroll Login as an enterprise enrollment?
   */
  Oobe.loginForTesting = function(
      username, password, gaia_id, enterpriseEnroll = false) {
    // Helper method that runs |fn| after |screenName| is visible.
    function waitForOobeScreen(screenName, fn) {
      if (Oobe.getInstance().currentScreen &&
          Oobe.getInstance().currentScreen.id === screenName) {
        fn();
      } else {
        $('oobe').addEventListener('screenchanged', function handler(e) {
          if (e.detail == screenName) {
            $('oobe').removeEventListener('screenchanged', handler);
            fn();
          }
        });
      }
    }

    chrome.send('skipToLoginForTesting');

    if (!enterpriseEnroll) {
      chrome.send('completeLogin', [gaia_id, username, password, false]);
    } else {
      waitForOobeScreen('gaia-signin', function() {
        // TODO(crbug.com/1100910): migrate logic to dedicated test api.
        chrome.send('toggleEnrollmentScreen');
        chrome.send('toggleFakeEnrollment');
      });

      waitForOobeScreen('enterprise-enrollment', function() {
        chrome.send('oauthEnrollCompleteLogin', [username]);
      });
    }
  };

  /**
   * Guest login for telemetry.
   */
  Oobe.guestLoginForTesting = function() {
    Oobe.skipToLoginForTesting();
    chrome.send('launchIncognito');
  };

  /**
   * Authenticate for telemetry - used for screenlocker.
   * @param {string} username Login username.
   * @param {string} password Login password.
   */
  Oobe.authenticateForTesting = function(username, password) {
    chrome.send('authenticateUser', [username, password, false]);
  };

  /**
   * Gaia login screen for telemetry.
   */
  Oobe.addUserForTesting = function() {
    Oobe.skipToLoginForTesting();
    chrome.send('addUser');
  };

  /**
   * Shows the add user dialog. Used in browser tests.
   */
  Oobe.showAddUserForTesting = function() {
    chrome.send('showAddUser');
  };

  /**
   * Hotrod requisition for telemetry.
   */
  Oobe.remoraRequisitionForTesting = function() {
    chrome.send('WelcomeScreen.setDeviceRequisition', ['remora']);
  };

  /**
   * Begin enterprise enrollment for telemetry.
   */
  Oobe.switchToEnterpriseEnrollmentForTesting = function() {
    // TODO(crbug.com/1100910): migrate logic to dedicated test api.
    chrome.send('toggleEnrollmentScreen');
  };

  /**
   * Finish enterprise enrollment for telemetry.
   */
  Oobe.enterpriseEnrollmentDone = function() {
    chrome.send('oauthEnrollClose', ['done']);
  };

  /**
   * Returns true if enrollment was successful. Dismisses the enrollment
   * attribute screen if it's present.
   *
   *  TODO(crbug.com/1111387) - Remove inline values from
   *  ENROLLMENT_STEP once fully migrated to JS modules.
   */
  Oobe.isEnrollmentSuccessfulForTest = function() {
    const step = $('enterprise-enrollment').uiStep;
    // See [ENROLLMENT_STEP.ATTRIBUTE_PROMPT]
    // from c/b/r/chromeos/login/enterprise_enrollment.js
    if (step === 'attribute-prompt') {
      chrome.send('oauthEnrollAttributes', ['', '']);
      return true;
    }

    // See [ENROLLMENT_STEP.SUCCESS]
    // from c/b/r/chromeos/login/enterprise_enrollment.js
    return step === 'success';
  };

  /**
   * Starts online demo mode setup for telemetry.
   */
  Oobe.setUpOnlineDemoModeForTesting = function() {
    DemoModeTestHelper.setUp('online');
  };

  /**
   * Changes some UI which depends on the virtual keyboard being shown/hidden.
   */
  Oobe.setVirtualKeyboardShown = function(shown) {
    Oobe.getInstance().virtualKeyboardShown = shown;
  };

  /**
   * Sets the current size of the client area (display size).
   * @param {number} width client area width
   * @param {number} height client area height
   */
  Oobe.setClientAreaSize = function(width, height) {
    Oobe.getInstance().setClientAreaSize(width, height);
  };

  /**
   * Sets the current height of the shelf area.
   * @param {number} height current shelf height
   */
  Oobe.setShelfHeight = function(height) {
    Oobe.getInstance().setShelfHeight(height);
  };

  Oobe.setOrientation = function(isHorizontal) {
    Oobe.getInstance().setOrientation(isHorizontal);
  };

  /**
   * Sets the required size of the oobe dialog.
   * @param {number} width oobe dialog width
   * @param {number} height oobe dialog height
   */
  Oobe.setDialogSize = function(width, height) {
    Oobe.getInstance().setDialogSize(width, height);
  };

  /**
   * Sets the hint for calculating OOBE dialog margins.
   * @param {OobeTypes.DialogPaddingMode} mode.
   */
  Oobe.setDialogPaddingMode = function(mode) {
    Oobe.getInstance().setDialogPaddingMode(mode);
  };

  /**
   * Get the primary display's name.
   *
   * Same as the displayInfo.name parameter returned by
   * chrome.system.display.getInfo(), but unlike chrome.system it's available
   * during OOBE.
   *
   * @return {string} The name of the primary display.
   */
  Oobe.getPrimaryDisplayNameForTesting = function() {
    return cr.sendWithPromise('getPrimaryDisplayNameForTesting');
  };

  /**
   * Click on the primary action button ("Next" usually).
   */
  Oobe.clickGaiaPrimaryButtonForTesting = function() {
    $('gaia-signin').clickPrimaryButtonForTesting();
  };

  /**
   * Sets the number of users on the views login screen.
   * @param {number} userCount The number of users.
   */
  Oobe.setLoginUserCount = function(userCount) {
    Oobe.getInstance().setLoginUserCount(userCount);
  };

  // Export
  return {Oobe: Oobe};
});

var Oobe = cr.ui.Oobe;

// Allow selection events on components with editable text (password field)
// bug (http://code.google.com/p/chromium/issues/detail?id=125863)
disableTextSelectAndDrag(function(e) {
  var src = e.target;
  return src instanceof HTMLTextAreaElement ||
      src instanceof HTMLInputElement && /text|password|search/.test(src.type);
});
