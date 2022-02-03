// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for smart privacy protection screen.
 */

/* #js_imports_placeholder */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {LoginScreenBehaviorInterface}
 * @implements {OobeI18nBehaviorInterface}
 */
const SmartPrivacyProtectionScreenElementBase = Polymer.mixinBehaviors(
    [OobeDialogHostBehavior, OobeI18nBehavior, LoginScreenBehavior],
    Polymer.Element);

/**
 * @polymer
 */
class SmartPrivacyProtectionScreen extends
    SmartPrivacyProtectionScreenElementBase {
  static get is() {
    return 'smart-privacy-protection-element';
  }

  /* #html_template_placeholder */

  static get properties() {
    return {
      /**
       * True if snooping protection is enabled.
       * @private
       */
      isSnoopingProtectionEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isSnoopingProtectionEnabled');
        },
        readOnly: true,
      },

      /**
       * True screen lock is enabled.
       * @private
       */
      isQuickDimEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isQuickDimEnabled');
        },
        readOnly: true,
      },

      /**
       * Indicates whether user is minor mode user (e.g. under age of 18).
       * @private
       */
      isMinorMode_: {
        type: Boolean,
        // TODO(dkuzmin): change the default value once appropriate capability
        // is available on C++ side.
        value: true,
      }
    };
  }

  get EXTERNAL_API() {
    return ['setIsMinorMode'];
  }

  ready() {
    super.ready();
    this.initializeLoginScreen('SmartPrivacyProtectionScreen', {
      resetAllowed: true,
    });
  }

  /**
   * Set the minor mode flag, which controls whether we could use nudge
   * techinuque on the UI.
   * @param {boolean} isMinorMode
   */
  setIsMinorMode(isMinorMode) {
    this.isMinorMode_ = isMinorMode;
  }

  onTurnOnButtonClicked_() {
    this.userActed('continue-feature-on');
  }

  onNoThanksButtonClicked_() {
    this.userActed('continue-feature-off');
  }
}

customElements.define(
    SmartPrivacyProtectionScreen.is, SmartPrivacyProtectionScreen);
