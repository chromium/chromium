// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Oobe signin screen implementation.
 */


import '../../components/notification_card.js';

import {assert} from '//resources/ash/common/assert.js';
import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.js';
import {MultiStepBehavior, MultiStepBehaviorInterface} from '../../components/behaviors/multi_step_behavior.js';
import {OobeI18nBehavior, OobeI18nBehaviorInterface} from '../../components/behaviors/oobe_i18n_behavior.js';

// The help topic regarding user not being in the allowlist.
const HELP_CANT_ACCESS_ACCOUNT = 188036;

/**
 * UI mode for the dialog.
 * @enum {string}
 */
const DialogMode = {
  DEFAULT: 'default',
};


/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {LoginScreenBehaviorInterface}
 * @implements {MultiStepBehaviorInterface}
 * @implements {OobeI18nBehaviorInterface}
 */
const UserAllowlistCheckScreenElementBase = mixinBehaviors(
    [OobeI18nBehavior, LoginScreenBehavior, MultiStepBehavior], PolymerElement);


/**
 * @polymer
 */
class UserAllowlistCheckScreenElement extends
    UserAllowlistCheckScreenElementBase {
  static get is() {
    return 'user-allowlist-check-screen-element';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * @private {string}
       */
      allowlistError_: {
        type: String,
        value: 'allowlistErrorConsumer',
      },
    };
  }

  get EXTERNAL_API() {
    return [];
  }

  defaultUIStep() {
    return DialogMode.DEFAULT;
  }

  get UI_STEPS() {
    return DialogMode;
  }

  /** @override */
  ready() {
    super.ready();
    this.initializeLoginScreen('UserAllowlistCheckScreen');
  }

  /**
   * Event handler that is invoked just before the frame is shown.
   * @param {!Object=} opt_data Optional additional information.
   */
  onBeforeShow(opt_data) {
    const isManaged = opt_data && opt_data.enterpriseManaged;
    const isFamilyLinkAllowed = opt_data && opt_data.familyLinkAllowed;
    if (isManaged && isFamilyLinkAllowed) {
      this.allowlistError_ = 'allowlistErrorEnterpriseAndFamilyLink';
    } else if (isManaged) {
      this.allowlistError_ = 'allowlistErrorEnterprise';
    } else {
      this.allowlistError_ = 'allowlistErrorConsumer';
    }

    this.$['gaia-allowlist-error'].submitButton.focus();
  }

  onAllowlistErrorTryAgainClick_() {
    this.userActed('retry');
  }

  onAllowlistErrorLinkClick_() {
    chrome.send('launchHelpApp', [HELP_CANT_ACCESS_ACCOUNT]);
  }
}

customElements.define(
    UserAllowlistCheckScreenElement.is, UserAllowlistCheckScreenElement);
