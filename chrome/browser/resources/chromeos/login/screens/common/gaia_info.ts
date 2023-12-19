// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/chromeos/cros_color_overrides.css.js';
import '//resources/cr_elements/cr_radio_button/cr_card_radio_button.js';
import '//resources/cr_elements/cr_radio_group/cr_radio_group.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/oobe_icons.html.js';
import '../../components/oobe_illo_icons.html.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/cr_card_radio_group_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';
import '../../components/oobe_cr_lottie.js';

import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.js';
import {MultiStepBehavior, MultiStepBehaviorInterface} from '../../components/behaviors/multi_step_behavior.js';
import {OobeI18nBehavior, OobeI18nBehaviorInterface} from '../../components/behaviors/oobe_i18n_behavior.js';
import {OOBE_UI_STATE} from '../../components/display_manager_types.js';
import type {OobeCrLottie} from '../../components/oobe_cr_lottie.js';

import {getTemplate} from './gaia_info.html.js';

export const GaiaInfoScreenElementBase =
    mixinBehaviors(
        [OobeI18nBehavior, LoginScreenBehavior, MultiStepBehavior],
        PolymerElement) as {
      new (): PolymerElement & OobeI18nBehaviorInterface &
          LoginScreenBehaviorInterface & MultiStepBehaviorInterface,
    };


enum GaiaInfoStep {
  OVERVIEW = 'overview',
}


enum UserCreationFlowType {
  MANUAL = 'manual',
  QUICKSTART = 'quickstart',
}


enum UserAction {
  BACK = 'back',
  MANUAL = 'manual',
  QUICKSTART = 'quickstart',
}


export class GaiaInfoScreen extends GaiaInfoScreenElementBase {
  static get is() {
    return 'gaia-info-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      /**
       * The currently selected flow type.
       */
      selectedFlowType_: {
        type: String,
        value: '',
      },
      /**
       * Whether Quick start feature is enabled. If it's enabled the quick start
       * button will be shown in the gaia info screen.
       */
      isQuickStartVisible_: {
        type: Boolean,
        value: false,
      },
    };
  }

  private selectedFlowType_: string;
  private isQuickStartVisible_: boolean;

  override get EXTERNAL_API(): string[] {
    return ['setQuickStartVisible'];
  }

  override get UI_STEPS() {
    return GaiaInfoStep;
  }

  onBeforeShow(): void {
    this.selectedFlowType_ = '';
    this.setAnimationPlaying_(true);
  }

  onBeforeHide(): void {
    this.setAnimationPlaying_(false);
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  override defaultUIStep(): GaiaInfoStep {
    return GaiaInfoStep.OVERVIEW;
  }

  override ready(): void {
    super.ready();
    this.initializeLoginScreen('GaiaInfoScreen');
  }

  setQuickStartVisible(): void {
    this.isQuickStartVisible_ = true;
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  override getOobeUIInitialState(): OOBE_UI_STATE {
    return OOBE_UI_STATE.GAIA_INFO;
  }

  private onNextClicked_(): void {
    if (this.isQuickStartVisible_ &&
        this.selectedFlowType_ == UserCreationFlowType.QUICKSTART) {
      this.userActed(UserAction.QUICKSTART);
    } else {
      this.userActed(UserAction.MANUAL);
    }
  }

  private onBackClicked_(): void {
    this.userActed(UserAction.BACK);
  }

  private isNextButtonEnabled_(
      isQuickStartVisible: boolean, selectedFlowType: string): boolean {
    return (!isQuickStartVisible) || selectedFlowType !== '';
  }

  private setAnimationPlaying_(play: boolean): void {
    const gaiaInfoAnimation =
        this.shadowRoot!.querySelector<OobeCrLottie>('#gaiaInfoAnimation');
    if (gaiaInfoAnimation) {
      gaiaInfoAnimation.playing = play;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [GaiaInfoScreen.is]: GaiaInfoScreen;
  }
}

customElements.define(GaiaInfoScreen.is, GaiaInfoScreen);
