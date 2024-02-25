// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for touchpad scroll screen.
 */

import '//resources/polymer/v3_0/iron-iconset-svg/iron-iconset-svg.js';
import '../../components/buttons/oobe_next_button.js';
import '../../components/buttons/oobe_text_button.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';
import '../../components/oobe_display_size_selector.js';
import '../../components/oobe_icons.html.js';

import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.js';
import {MultiStepBehavior, MultiStepBehaviorInterface} from '../../components/behaviors/multi_step_behavior.js';
import {OobeI18nBehavior, OobeI18nBehaviorInterface} from '../../components/behaviors/oobe_i18n_behavior.js';
import {OobeUiState} from '../../components/display_manager_types.js';
import type {OobeDisplaySizeSelector} from '../../components/oobe_display_size_selector.js';

import {getTemplate} from './display_size.html.js';

export const DisplaySizeScreenElementBase =
    mixinBehaviors(
        [OobeI18nBehavior, LoginScreenBehavior, MultiStepBehavior],
        PolymerElement) as {
      new (): PolymerElement & OobeI18nBehaviorInterface &
          LoginScreenBehaviorInterface & MultiStepBehaviorInterface,
    };


/**
 * Enum to represent steps on the display size screen.
 * Currently there is only one step, but we still use
 * MultiStepBehavior because it provides implementation of
 * things like processing 'focus-on-show' class
 */
enum DisplaySizeStep {
  OVERVIEW = 'overview',
}

/**
 * Available user actions.
 */
enum UserAction {
  NEXT = 'next',
  RETURN = 'return',
}

interface DisplaySizeScreenData {
  availableSizes: number[];
  currentSize: number;
  shouldShowReturn: boolean;
}

class DisplaySizeScreen extends DisplaySizeScreenElementBase {
  static get is() {
    return 'display-size-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      shouldShowReturn_: {
        type: Boolean,
        value: false,
      },
    };
  }

  private shouldShowReturn_: boolean;

  override get UI_STEPS() {
    return DisplaySizeStep;
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  override defaultUIStep() {
    return DisplaySizeStep.OVERVIEW;
  }

  override ready(): void {
    super.ready();
    this.initializeLoginScreen('DisplaySizeScreen');
  }

  /**
   * @param {DisplaySizeScreenData} data Screen init payload.
   */
  onBeforeShow(data: DisplaySizeScreenData): void {
    this.shadowRoot!.querySelector<OobeDisplaySizeSelector>('#sizeSelector')!
        .init(data['availableSizes'], data['currentSize']);
    this.shouldShowReturn_ = data['shouldShowReturn'];
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  override getOobeUIInitialState(): OobeUiState {
    return OobeUiState.CHOOBE;
  }

  private onNextClicked_(): void {
    this.userActed([
      UserAction.NEXT,
      this.shadowRoot!.querySelector<OobeDisplaySizeSelector>(
                          '#sizeSelector')!.getSelectedSize(),
    ]);
  }

  private onReturnClicked_(): void {
    this.userActed([
      UserAction.RETURN,
      this.shadowRoot!.querySelector<OobeDisplaySizeSelector>(
                          '#sizeSelector')!.getSelectedSize(),
    ]);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [DisplaySizeScreen.is]: DisplaySizeScreen;
  }
}

customElements.define(DisplaySizeScreen.is, DisplaySizeScreen);
