// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Oobe signin screen implementation.
 */


import '../../components/notification_card.js';

import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.js';
import {MultiStepBehavior, MultiStepBehaviorInterface} from '../../components/behaviors/multi_step_behavior.js';
import {OobeI18nBehavior, OobeI18nBehaviorInterface} from '../../components/behaviors/oobe_i18n_behavior.js';
import {GaiaButton} from '../../components/gaia_button.js';

import {getTemplate} from './user_allowlist_check_screen.html.js';

// The help topic regarding user not being in the allowlist.
const HELP_CANT_ACCESS_ACCOUNT = 188036;

/**
 * UI mode for the dialog.
 */
enum DialogMode {
  DEFAULT = 'default',
}

const UserAllowlistCheckScreenElementBase =
    mixinBehaviors(
        [OobeI18nBehavior, LoginScreenBehavior, MultiStepBehavior],
        PolymerElement) as {
      new (): PolymerElement & OobeI18nBehaviorInterface &
          LoginScreenBehaviorInterface & MultiStepBehaviorInterface,
    };

/**
 * Data that is passed to the screen during onBeforeShow.
 */
interface UserAllowlistCheckScreenData {
  enterpriseManaged: boolean;
  familyLinkAllowed: boolean;
}

export class UserAllowlistCheckScreenElement extends
    UserAllowlistCheckScreenElementBase {
  static get is() {
    return 'user-allowlist-check-screen-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      allowlistError: {
        type: String,
        value: 'allowlistErrorConsumer',
      },
    };
  }

  private allowlistError: string;

  override get EXTERNAL_API() {
    return [];
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  override defaultUIStep(): DialogMode {
    return DialogMode.DEFAULT;
  }

  override get UI_STEPS() {
    return DialogMode;
  }

  override ready(): void {
    super.ready();
    this.initializeLoginScreen('UserAllowlistCheckScreen');
  }

  /**
   * Event handler that is invoked just before the frame is shown.
   */
  onBeforeShow(optData: UserAllowlistCheckScreenData): void {
    const isManaged = optData && optData.enterpriseManaged;
    const isFamilyLinkAllowed = optData && optData.familyLinkAllowed;
    if (isManaged && isFamilyLinkAllowed) {
      this.allowlistError = 'allowlistErrorEnterpriseAndFamilyLink';
    } else if (isManaged) {
      this.allowlistError = 'allowlistErrorEnterprise';
    } else {
      this.allowlistError = 'allowlistErrorConsumer';
    }

    const submitButton =
        this.shadowRoot?.querySelector<GaiaButton>('#submitButton');
    if (submitButton instanceof GaiaButton) {
      submitButton.focus();
    }
  }

  private onAllowlistErrorTryAgainClick(): void {
    this.userActed('retry');
  }

  private onAllowlistErrorLinkClick_(): void {
    chrome.send('launchHelpApp', [HELP_CANT_ACCESS_ACCOUNT]);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [UserAllowlistCheckScreenElement.is]: UserAllowlistCheckScreenElement;
  }
}

customElements.define(
    UserAllowlistCheckScreenElement.is, UserAllowlistCheckScreenElement);
