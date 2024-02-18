// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @fileoverview Polymer element for theme selection screen.
 */

import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '//resources/polymer/v3_0/iron-iconset-svg/iron-iconset-svg.js';
import '../../components/buttons/oobe_next_button.js';
import '../../components/buttons/oobe_text_button.js';
import '../../components/oobe_icons.html.js';
import '../../components/oobe_screens_list.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';

import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.js';
import {MultiStepBehavior, MultiStepBehaviorInterface} from '../../components/behaviors/multi_step_behavior.js';
import {OobeI18nBehavior, OobeI18nBehaviorInterface} from '../../components/behaviors/oobe_i18n_behavior.js';
import {OobeUiState} from '../../components/display_manager_types.js';
import {OobeScreensList, OobeScreensListData} from '../../components/oobe_screens_list.js';

import {getTemplate} from './choobe.html.js';

export const ChoobeScreenElementBase =
  mixinBehaviors(
      [OobeI18nBehavior, LoginScreenBehavior, MultiStepBehavior],
      PolymerElement) as {
    new (): PolymerElement & OobeI18nBehaviorInterface &
        LoginScreenBehaviorInterface & MultiStepBehaviorInterface,
  };

enum ChoobeStep {
  OVERVIEW = 'overview',
}

/**
 * Available user actions.
 */
enum UserAction {
  SKIP = 'choobeSkip',
  NEXT = 'choobeSelect',
}

interface ChoobeScreenData {
  screens: OobeScreensListData;
}

class ChoobeScreen extends ChoobeScreenElementBase {
  static get is() {
    return 'choobe-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      numberOfSelectedScreens_: {
        type: Number,
        value: 0,
      },
    };
  }

  private numberOfSelectedScreens_: number;

  override get UI_STEPS() {
    return ChoobeStep;
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  override defaultUIStep() {
    return ChoobeStep.OVERVIEW;
  }

  override ready(): void {
    super.ready();
    this.initializeLoginScreen('ChoobeScreen');
  }

  onBeforeShow(data: ChoobeScreenData): void {
    if ('screens' in data) {
      this.shadowRoot!.querySelector<OobeScreensList>('#screensList')!.init(
        data['screens']);
    }
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  override getOobeUIInitialState(): OobeUiState {
    return OobeUiState.CHOOBE;
  }

  private onNextClicked_(): void {
    const screenSelected =
        this.shadowRoot!.querySelector<OobeScreensList>(
                            '#screensList')!.getScreenSelected();
    this.userActed([UserAction.NEXT, screenSelected]);
  }

  private onSkip_(): void {
    this.userActed(UserAction.SKIP);
  }

  private canProceed_(): boolean {
    return this.numberOfSelectedScreens_ > 0;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [ChoobeScreen.is]: ChoobeScreen;
  }
}

customElements.define(ChoobeScreen.is, ChoobeScreen);
