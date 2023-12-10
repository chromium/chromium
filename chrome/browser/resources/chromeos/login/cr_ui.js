// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Common OOBE controller methods for use in OOBE and login.
 * This file is shared between OOBE and login. Add only methods that need to be
 * shared between all *two* screens here.
 */

import '//resources/js/cr.js';

import {assert} from '//resources/ash/common/assert.js';
import {$} from '//resources/ash/common/util.js';

import {OobeTypes} from './components/oobe_types.js';
import {DisplayManager} from './display_manager.js';
import {loadTimeData} from './i18n_setup.js';

/** @type {?Oobe} */
let instance = null;

/**
 * Out of box controller. It manages initialization of screens,
 * transitions, error messages display.
 */
export class Oobe extends DisplayManager {
  /** @return {!Oobe} */
  static getInstance() {
    return instance || (instance = new Oobe());
  }

  /**
   * Handle the cancel accelerator.
   */
  static handleCancel() {
    Oobe.getInstance().handleCancel();
  }

  /**
   * Shows the given screen.
   * @param {Object} screen Screen params dict, e.g. {id: screenId,
   *   data: data}
   */
  static showScreen(screen) {
    Oobe.getInstance().showScreen(screen);
  }

  /**
   * Toggles system info visibility.
   */
  static toggleSystemInfo() {
    Oobe.getInstance().toggleSystemInfo();
  }

  /**
   * Does the initial transition to the OOBE flow after booting animation.
   */
  static triggerDown() {
    // Notify that we are going to play initial animation in the WebUI.
    document.dispatchEvent(new CustomEvent('about-to-shrink'));
    // Delay this call to reduce the load during animation.
    setTimeout(() => Oobe.getInstance().triggerDown(), 0);
  }

  /**
   * Update body class to switch between OOBE UI and Login UI.
   * @param {boolean} showOobe True if UI is in an OOBE mode (as opposed to
   * login).
   */
  static showOobeUI(showOobe) {
    if (showOobe) {
      document.body.classList.add('oobe-display');
    } else {
      document.body.classList.remove('oobe-display');
    }
  }

  /**
   * Enables keyboard driven flow.
   * @param {boolean} value True if keyboard navigation flow is forced.
   */
  static enableKeyboardFlow(value) {
    Oobe.getInstance().forceKeyboardFlow = value;
  }

  /**
   * Changes some UI which depends on the virtual keyboard being shown/hidden.
   */
  static setVirtualKeyboardShown(shown) {
    Oobe.getInstance().virtualKeyboardShown = shown;
  }

  /**
   * Sets the current height of the shelf area.
   * @param {number} height current shelf height
   */
  static setShelfHeight(height) {
    Oobe.getInstance().setShelfHeight(height);
  }

  static setOrientation(isHorizontal) {
    Oobe.getInstance().setOrientation(isHorizontal);
  }

  /**
   * Sets the required size of the oobe dialog.
   * @param {number} width oobe dialog width
   * @param {number} height oobe dialog height
   */
  static setDialogSize(width, height) {
    Oobe.getInstance().setDialogSize(width, height);
  }

  /**
   * Login for telemetry.
   * @param {string} username Login username.
   * @param {string} password Login password.
   * @param {string} gaia_id GAIA ID.
   * @param {boolean} enterpriseEnroll Login as an enterprise enrollment?
   */
  static loginForTesting(
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

    chrome.send('OobeTestApi.skipToLoginForTesting');

    if (!enterpriseEnroll) {
      chrome.send('completeLogin', [gaia_id, username, password, false]);
    } else {
      waitForOobeScreen('gaia-signin', function() {
        // TODO(crbug.com/1100910): migrate logic to dedicated test api.
        chrome.send('OobeTestApi.advanceToScreen', ['enterprise-enrollment']);
      });

      waitForOobeScreen('enterprise-enrollment', function() {
        chrome.send(
            'toggleFakeEnrollmentAndCompleteLogin',
            [username, OobeTypes.LicenseType.ENTERPRISE],
        );
      });
    }
  }  // loginForTesting

  /**
   * Returns true if enrollment was successful. Dismisses the enrollment
   * attribute screen if it's present.
   *
   * @suppress {missingProperties}
   * $('enterprise-enrollment').uiStep
   * TODO(crbug.com/1229130) - Remove this suppression.
   */
  static isEnrollmentSuccessfulForTest() {
    const step = $('enterprise-enrollment').uiStep;
    // TODO(crbug.com/1229130) - Improve this check.
    if (step === OobeTypes.EnrollmentStep.ATTRIBUTE_PROMPT) {
      chrome.send('oauthEnrollAttributes', ['', '']);
      return true;
    }

    return step === OobeTypes.EnrollmentStep.SUCCESS;
  }

  /**
   * Click on the primary action button ("Next" usually) for Gaia. On the
   * Login or Enterprise Enrollment screen.
   *
   * @suppress {missingProperties}
   * $('...').clickPrimaryButtonForTesting()
   * TODO(crbug.com/1229130) - Remove this suppression.
   */
  static clickGaiaPrimaryButtonForTesting() {
    if (!$('gaia-signin').hidden) {
      $('gaia-signin').clickPrimaryButtonForTesting();
    } else {
      assert(!$('enterprise-enrollment').hidden);
      $('enterprise-enrollment').clickPrimaryButtonForTesting();
    }
  }
  /**
   * Initializes the OOBE flow.  This will cause all C++ handlers to
   * be invoked to do final setup.
   */
  static initialize() {
    Oobe.getInstance().initialize();
    chrome.send('screenStateInitialize');
  }

  /**
   * Reloads content of the page (localized strings, options of the select
   * controls).
   * @param {!Object} data New dictionary with i18n values.
   */
  static reloadContent(data) {
    // Reload global local strings, process DOM tree again.
    loadTimeData.overrideValues(data);
    Oobe.updateDocumentLocalizedStrings();

    // Update localized content of the screens.
    Oobe.getInstance().updateLocalizedContent_();
  }

  /**
   * Update localized strings in tags that are used at the `document` level.
   * These strings are used outside of a Polymer Element and cannot leverage
   * I18nBehavior for it.
   */
  static updateDocumentLocalizedStrings() {
    // Update attributes used in the <html> tag.
    const attrToStrMap = {
      lang: 'language',
      dir: 'textdirection',
      highlight: 'highlightStrength',
      tablet: 'isInTabletMode',
    };
    for (const [attribute, stringName] of Object.entries(attrToStrMap)) {
      const localizedString = loadTimeData.getValue(stringName);
      document.documentElement.setAttribute(attribute, localizedString);
    }

    $('api-keys-notice').updateLocaleAndMaybeShowNotice();
  }

  /**
   * Updates "device in tablet mode" state when tablet mode is changed.
   * @param {boolean} isInTabletMode True when in tablet mode.
   */
  static setTabletModeState(isInTabletMode) {
    Oobe.getInstance().setTabletModeState_(isInTabletMode);
  }

  /**
   * Updates OOBE configuration when it is loaded.
   * @param {!OobeTypes.OobeConfiguration} configuration OOBE configuration.
   */
  static updateOobeConfiguration(configuration) {
    Oobe.getInstance().updateOobeConfiguration_(configuration);
  }

}  // class Oobe

/**
 * Some ForTesting APIs directly access to DOM. Because this script is loaded
 * in header, DOM tree may not be available at beginning.
 * In DOMContentLoaded, after Oobe.initialize() is done, this is marked to
 * true, indicating ForTesting methods can be called.
 * External script using ForTesting APIs should wait for this condition.
 * @type {boolean}
 */
Oobe.readyForTesting = false;
