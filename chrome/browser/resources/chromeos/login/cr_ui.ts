// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Common OOBE controller methods for use in OOBE and login.
 * This file is shared between OOBE and login. Add only methods that need to be
 * shared between all *two* screens here.
 */

import '//resources/js/cr.js';

import {assert} from '//resources/js/assert.js';

import {ApiKeysNoticeElement} from './components/api_keys_notice.js';
import {OobeTypes} from './components/oobe_types.js';
import {DisplayManager} from './display_manager.js';
import {loadTimeData} from './i18n_setup.js';
import {GaiaSigninElement} from './screens/common/gaia_signin.js';
import {EnterpriseEnrollmentElement} from './screens/oobe/enterprise_enrollment.js';

let instance: Oobe|null = null;

declare global {
  interface HTMLElementEventMap {
    'screenchanged': CustomEvent<string>;
  }
}

/**
 * Out of box controller. It manages initialization of screens,
 * transitions, error messages display.
 */
export class Oobe extends DisplayManager {
  static readyForTesting = false;

  static getInstance(): Oobe {
    return instance || (instance = new Oobe());
  }

  /**
   * Handle the cancel accelerator.
   */
  static handleCancel(): void {
    Oobe.getInstance().handleCancel();
  }

  /**
   * Shows the given screen.
   * TODO(b/322313099): Either update data type to use some base screen data
   * class or make `showScreen` to have only screen id as a parameter.
   */
  static showScreen(screen: {id: string, data: any}): void {
    Oobe.getInstance().showScreen(screen);
  }

  /**
   * Toggles system info visibility.
   */
  static toggleSystemInfo(): void {
    Oobe.getInstance().toggleSystemInfo();
  }

  /**
   * Does the initial transition to the OOBE flow after booting animation.
   */
  static triggerDown(): void {
    // Notify that we are going to play initial animation in the WebUI.
    document.dispatchEvent(new CustomEvent('about-to-shrink'));
    // Delay this call to reduce the load during animation.
    setTimeout(() => Oobe.getInstance().triggerDown(), 0);
  }

  /**
   * Enables keyboard driven flow.
   * @param value True if keyboard navigation flow is forced.
   */
  static enableKeyboardFlow(value: boolean): void {
    Oobe.getInstance().forceKeyboardFlow = value;
  }

  /**
   * Changes some UI which depends on the virtual keyboard being shown/hidden.
   */
  static setVirtualKeyboardShown(shown: boolean): void {
    Oobe.getInstance().setVirtualKeyboardShown(shown);
  }

  /**
   * Sets the current height of the shelf area.
   * @param height current shelf height
   */
  static setShelfHeight(height: number): void {
    Oobe.getInstance().setShelfHeight(height);
  }

  static setOrientation(isHorizontal: boolean): void {
    Oobe.getInstance().setOrientation(isHorizontal);
  }

  /**
   * Sets the required size of the oobe dialog.
   * @param width oobe dialog width
   * @param height oobe dialog height
   */
  static setDialogSize(width: number, height: number): void {
    Oobe.getInstance().setDialogSize(width, height);
  }

  /**
   * Login for telemetry.
   * @param username Login username.
   * @param password Login password.
   * @param gaiaId GAIA ID.
   * @param enterpriseEnroll Login as an enterprise enrollment?
   */
  static loginForTesting(
      username: string, password: string, gaiaId: string,
      enterpriseEnroll: boolean = false): void {
    // Helper method that runs |fn| after |screenName| is visible.
    function waitForOobeScreen(screenName: string, fn: () => void) {
      const currentScreen = Oobe.getInstance().currentScreen;
      if (currentScreen && currentScreen.id === screenName) {
        fn();
      } else {
        const oobe = document.querySelector('#oobe');
        assert(oobe instanceof HTMLElement);
        oobe.addEventListener(
            'screenchanged', function handler(e: CustomEvent<string>): void {
              if (e.detail === screenName) {
                oobe.removeEventListener('screenchanged', handler);
                fn();
              }
            });
      }
    }

    chrome.send('OobeTestApi.skipToLoginForTesting');

    if (!enterpriseEnroll) {
      chrome.send('OobeTestApi.completeLogin', [gaiaId, username, password]);
    } else {
      waitForOobeScreen('gaia-signin', function(): void {
        chrome.send('OobeTestApi.advanceToScreen', ['enterprise-enrollment']);
      });

      waitForOobeScreen('enterprise-enrollment', function(): void {
        // TODO(b/260015541): migrate logic to dedicated test api.
        chrome.send(
            'toggleFakeEnrollmentAndCompleteLogin',
            [
              username,
              gaiaId,
              password,
              /*using_saml*/ false,
              OobeTypes.LicenseType.ENTERPRISE,
            ],
        );
      });
    }
  }

  /**
   * Returns true if enrollment was successful. Dismisses the enrollment
   * attribute screen if it's present.
   */
  static isEnrollmentSuccessfulForTest(): boolean {
    const step = document
                     .querySelector<EnterpriseEnrollmentElement>(
                         '#enterprise-enrollment')
                     ?.uiStep;
    // TODO(crbug.com/1229130) - Improve this check.
    if (step === OobeTypes.EnrollmentStep.ATTRIBUTE_PROMPT) {
      // TODO(b/260015541): migrate logic to dedicated test api.
      chrome.send('oauthEnrollAttributes', ['', '']);
      return true;
    }

    return step === OobeTypes.EnrollmentStep.SUCCESS;
  }

  /**
   * Click on the primary action button ("Next" usually) for Gaia. On the
   * Login or Enterprise Enrollment screen.
   */
  static clickGaiaPrimaryButtonForTesting(): void {
    const gaiaSignin = document.querySelector('#gaia-signin');
    if (gaiaSignin instanceof GaiaSigninElement && !gaiaSignin.hidden) {
      gaiaSignin.clickPrimaryButtonForTesting();
    } else {
      const enterpriseEnrollment =
          document.querySelector('#enterprise-enrollment');
      assert(
          enterpriseEnrollment instanceof EnterpriseEnrollmentElement &&
          !enterpriseEnrollment.hidden);
      enterpriseEnrollment.clickPrimaryButtonForTesting();
    }
  }
  /**
   * Initializes the OOBE flow.  This will cause all C++ handlers to
   * be invoked to do final setup.
   */
  static initialize(): void {
    Oobe.getInstance().initialize();
    chrome.send('screenStateInitialize');
  }

  /**
   * Reloads content of the page (localized strings, options of the select
   * controls).
   * @param data New dictionary with i18n values.
   */
  static reloadContent(data: {[key: string]: string}): void {
    // Reload global local strings, process DOM tree again.
    loadTimeData.overrideValues(data);
    Oobe.updateDocumentLocalizedStrings();

    // Update localized content of the screens.
    Oobe.getInstance().updateLocalizedContent();
  }

  /**
   * Update localized strings in tags that are used at the `document` level.
   * These strings are used outside of a Polymer Element and cannot leverage
   * I18nMixin for it.
   */
  static updateDocumentLocalizedStrings(): void {
    // Update attributes used in the <html> tag.
    const attrToStrMap = {
      lang: 'language',
      dir: 'textdirection',
      highlight: 'highlightStrength',
    };
    for (const [attribute, stringName] of Object.entries(attrToStrMap)) {
      const localizedString = loadTimeData.getValue(stringName);
      document.documentElement.setAttribute(attribute, localizedString);
    }

    document.querySelector<ApiKeysNoticeElement>('#api-keys-notice')
        ?.updateLocaleAndMaybeShowNotice();
  }

  /**
   * Updates "device in tablet mode" state when tablet mode is changed.
   * @param isInTabletMode True when in tablet mode.
   */
  static setTabletModeState(isInTabletMode: boolean): void {
    document.documentElement.toggleAttribute('tablet', isInTabletMode);
  }

  /**
   * Updates OOBE configuration when it is loaded.
   * @param configuration OOBE configuration.
   */
  static updateOobeConfiguration(configuration: OobeTypes.OobeConfiguration):
      void {
    Oobe.getInstance().updateOobeConfiguration(configuration);
  }

}  // class Oobe

/**
 * Some ForTesting APIs directly access to DOM. Because this script is loaded
 * in header, DOM tree may not be available at beginning.
 * In DOMContentLoaded, after Oobe.initialize() is done, this is marked to
 * true, indicating ForTesting methods can be called.
 * External script using ForTesting APIs should wait for this condition.
 */
Oobe.readyForTesting = false;
