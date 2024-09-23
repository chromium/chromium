// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for signin fatal error.
 */

import '//resources/js/action_link.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/oobe_icons.html.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/buttons/oobe_text_button.js';

import {assert} from '//resources/js/assert.js';
import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {OobeAdaptiveDialog} from '../../components/dialogs/oobe_adaptive_dialog.js';
import {OobeUiState} from '../../components/display_manager_types.js';
import {LoginScreenMixin} from '../../components/mixins/login_screen_mixin.js';
import {OobeDialogHostMixin} from '../../components/mixins/oobe_dialog_host_mixin.js';
import {OobeI18nMixin} from '../../components/mixins/oobe_i18n_mixin.js';

import {getTemplate} from './factor_setup_success.html.js';

const FactorSetupSuccessBase =
    OobeDialogHostMixin(LoginScreenMixin(OobeI18nMixin(PolymerElement)));

// LINT.IfChange
/**
 * Set of modified factors, determine title/subtitle.
 */
enum ModifiedFactors {
  ONLINE_PASSWORD = 'online',
  LOCAL_PASSWORD = 'local',
  ONLINE_PASSWORD_AND_PIN = 'online+pin',
  LOCAL_PASSWORD_AND_PIN = 'local+pin',
  PIN = 'pin',
}

/**
 * Determines if factors were changed as a part of
 * initial setup (set) or during recovery (updated)
 */
enum ChangeMode {
  INITIAL_SETUP = 'set',
  RECOVERY_FLOW = 'update',
}


const ACTION_PROCEED = 'proceed';
// LINT.ThenChange(/chrome/browser/ash/login/screens/osauth/factor_setup_success_screen.cc)

interface FactorSetupSuccessScreenData {
  modifiedFactors: ModifiedFactors;
  changeMode: ChangeMode;
}

export class FactorSetupSuccessScreen extends FactorSetupSuccessBase {
  static get is() {
    return 'factor-setup-success-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      /**
       */
      hasNextStep: {
        type: Boolean,
        value: true,
      },
      /**
       */
      buttonsEnabled: {
        type: Boolean,
        value: false,
      },
      /**
       */
      factors: {
        type: String,
        value: ModifiedFactors.ONLINE_PASSWORD,
      },
      /**
       */
      changeMode: {
        type: String,
        value: ChangeMode.INITIAL_SETUP,
      },
    };
  }

  private hasNextStep: boolean;
  private buttonsEnabled: boolean;
  private factors: ModifiedFactors;
  private changeMode: ChangeMode;

  override ready(): void {
    super.ready();
    this.initializeLoginScreen('FactorSetupSuccessScreen');
  }

  /** Initial UI State for screen */
  // eslint-disable-next-line @typescript-eslint/naming-convention
  override getOobeUIInitialState(): OobeUiState {
    return OobeUiState.BLOCKING;
  }

  /** Returns a control which should receive an initial focus. */
  override get defaultControl(): HTMLElement {
    const dialog =  this.shadowRoot?.querySelector('#factorSetupSuccessDialog');
    assert(dialog instanceof OobeAdaptiveDialog);
    return dialog;
  }

  /**
   * Invoked just before being shown. Contains all the data for the screen.
   */
  override onBeforeShow(data: FactorSetupSuccessScreenData): void {
    super.onBeforeShow(data);
    this.factors = data['modifiedFactors'];
    this.changeMode = data['changeMode'];
    this.hasNextStep = this.changeMode === ChangeMode.INITIAL_SETUP;
    this.buttonsEnabled = true;
  }

  private getTitle(
      locale: string, factors: ModifiedFactors,
      changeMode: ChangeMode): string {
    if (changeMode === ChangeMode.INITIAL_SETUP) {
      if (factors === ModifiedFactors.LOCAL_PASSWORD) {
        return this.i18nDynamic(locale, 'factorSuccessTitleLocalPasswordSet');
      }
      // Add more strings here once we support more combinations of factors.
      // Fallback option:
      return this.i18nDynamic(locale, 'factorSuccessTitleLocalPasswordSet');
    } else {
      if (factors === ModifiedFactors.LOCAL_PASSWORD) {
        return this.i18nDynamic(
            locale, 'factorSuccessTitleLocalPasswordUpdated');
      }
      // Add more strings here once we support more combinations of factors.
      // Fallback option:
      return this.i18nDynamic(locale, 'factorSuccessTitleLocalPasswordUpdated');
    }
  }

  private getSubtitle(locale: string, factors: ModifiedFactors): string
      |undefined {
    if (factors === ModifiedFactors.LOCAL_PASSWORD) {
      return this.i18nDynamic(locale, 'factorSuccessSubtitleLocalPassword');
    }
    return undefined;
  }

  private onProceed(): void {
    if (!this.buttonsEnabled) {
      return;
    }
    this.buttonsEnabled = false;
    this.userActed(ACTION_PROCEED);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [FactorSetupSuccessScreen.is]: FactorSetupSuccessScreen;
  }
}

customElements.define(FactorSetupSuccessScreen.is, FactorSetupSuccessScreen);
