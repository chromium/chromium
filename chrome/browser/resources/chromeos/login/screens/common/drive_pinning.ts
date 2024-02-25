// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for drive pinning screen.
 */

import '//resources/ash/common/cr_elements/cros_color_overrides.css.js';
import '//resources/polymer/v3_0/iron-iconset-svg/iron-iconset-svg.js';
import '../../components/buttons/oobe_next_button.js';
import '../../components/oobe_icons.html.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';

import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.js';
import {MultiStepBehavior, MultiStepBehaviorInterface} from '../../components/behaviors/multi_step_behavior.js';
import {OobeI18nBehavior, OobeI18nBehaviorInterface} from '../../components/behaviors/oobe_i18n_behavior.js';
import {OobeUiState} from '../../components/display_manager_types.js';

import {getTemplate} from './drive_pinning.html.js';

export const DrivePinningScreenElementBase =
  mixinBehaviors(
      [OobeI18nBehavior, LoginScreenBehavior, MultiStepBehavior],
      PolymerElement) as {
    new (): PolymerElement & OobeI18nBehaviorInterface &
        LoginScreenBehaviorInterface & MultiStepBehaviorInterface,
  };

interface DrivePinningScreenData {
  shouldShowReturn: boolean;
}

/**
 * Enum to represent steps on the drive pinning screen.
 * Currently there is only one step, but we still use
 * MultiStepBehavior because it provides implementation of
 * things like processing 'focus-on-show' class
 */
enum DrivePinningStep {
  OVERVIEW = 'overview',
}

/**
 * Available user actions.
 */
enum UserAction {
  ACCEPT = 'driveNext',
  RETURN = 'return',
}

class DrivePinningScreen extends DrivePinningScreenElementBase {
  static get is() {
    return 'drive-pinning-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      /**
       * Available free space in the disk.
       */
      freeSpace_: {
        type: String,
      },

      /**
       * Required space by the drive for pinning.
       */
      requiredSpace_: {
        type: String,
      },

      enableDrivePinning_: {
        type: Boolean,
        value: true,
      },

      /**
       * Whether the button to return to CHOOBE screen should be shown.
       */
      shouldShowReturn_: {
        type: Boolean,
        value: false,
      },
    };
  }

  private freeSpace_: string;
  private requiredSpace_: string;
  private enableDrivePinning_: boolean;
  private shouldShowReturn_: boolean;

  override get EXTERNAL_API(): string[] {
    return ['setRequiredSpaceInfo'];
  }

  override get UI_STEPS() {
    return DrivePinningStep;
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  override defaultUIStep() {
    return DrivePinningStep.OVERVIEW;
  }

  override ready(): void {
    super.ready();
    this.initializeLoginScreen('DrivePinningScreen');
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  override getOobeUIInitialState(): OobeUiState {
    return OobeUiState.ONBOARDING;
  }

  private onBeforeShow(data: DrivePinningScreenData): void {
    this.shouldShowReturn_ = data['shouldShowReturn'];
  }

  private getSpaceDescription_(
      locale: string, requiredSpace: string, freeSpace: string): string {
    if (requiredSpace && freeSpace) {
      return this.i18nDynamic(
          locale, 'DevicePinningScreenToggleSubtitle', requiredSpace,
          freeSpace);
    }
    return '';
  }

  /**
   * Set the required space and free space information.
   */
  setRequiredSpaceInfo(requiredSpace: string, freeSpace: string): void {
    this.requiredSpace_ = requiredSpace;
    this.freeSpace_ = freeSpace;
  }

  private onNextButtonClicked_(): void {
    this.userActed([UserAction.ACCEPT, this.enableDrivePinning_]);
  }

  private onReturnClicked_(): void {
    this.userActed([UserAction.RETURN, this.enableDrivePinning_]);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [DrivePinningScreen.is]: DrivePinningScreen;
  }
}

customElements.define(DrivePinningScreen.is, DrivePinningScreen);
