// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for smart privacy protection screen.
 */

import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '//resources/polymer/v3_0/iron-iconset-svg/iron-iconset-svg.js';
import '../../components/oobe_icons.html.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.js';
import {OobeDialogHostBehavior, OobeDialogHostBehaviorInterface} from '../../components/behaviors/oobe_dialog_host_behavior.js';
import {OobeI18nMixin, OobeI18nMixinInterface} from '../../components/mixins/oobe_i18n_mixin.js';

import {getTemplate} from './smart_privacy_protection.html.js';



export const SmartPrivacyProtectionScreenElementBase =
    mixinBehaviors(
        [LoginScreenBehavior, OobeDialogHostBehavior],
        OobeI18nMixin(PolymerElement)) as {
      new (): PolymerElement & OobeI18nMixinInterface &
          LoginScreenBehaviorInterface & OobeDialogHostBehaviorInterface,
    };


export class SmartPrivacyProtectionScreen extends
    SmartPrivacyProtectionScreenElementBase {
  static get is() {
    return 'smart-privacy-protection-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      /**
       * True screen lock is enabled.
       */
      isQuickDimEnabled: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isQuickDimEnabled');
        },
        readOnly: true,
      },

      /**
       * Indicates whether user is minor mode user (e.g. under age of 18).
       */
      isMinorMode: {
        type: Boolean,
        // TODO(dkuzmin): change the default value once appropriate capability
        // is available on C++ side.
        value: true,
      },
    };
  }

  private isQuickDimEnabled: boolean;
  private isMinorMode: boolean;

  override get EXTERNAL_API(): string[] {
    return ['setIsMinorMode'];
  }

  override ready(): void {
    super.ready();
    this.initializeLoginScreen('SmartPrivacyProtectionScreen');
  }

  /**
   * Set the minor mode flag, which controls whether we could use nudge
   * techinuque on the UI.
   */
  setIsMinorMode(isMinorMode: boolean): void {
    this.isMinorMode = isMinorMode;
  }

  private onTurnOnButtonClicked(): void {
    this.userActed('continue-feature-on');
  }

  private onNoThanksButtonClicked(): void {
    this.userActed('continue-feature-off');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SmartPrivacyProtectionScreen.is]: SmartPrivacyProtectionScreen;
  }
}

customElements.define(
    SmartPrivacyProtectionScreen.is, SmartPrivacyProtectionScreen);
