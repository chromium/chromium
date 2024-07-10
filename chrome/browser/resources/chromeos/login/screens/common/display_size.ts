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
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {OobeUiState} from '../../components/display_manager_types.js';
import {LoginScreenMixin} from '../../components/mixins/login_screen_mixin.js';
import {MultiStepMixin} from '../../components/mixins/multi_step_mixin.js';
import {OobeI18nMixin} from '../../components/mixins/oobe_i18n_mixin.js';
import type {OobeDisplaySizeSelector} from '../../components/oobe_display_size_selector.js';

import {getTemplate} from './display_size.html.js';

const DisplaySizeScreenElementBase =
    LoginScreenMixin(MultiStepMixin(OobeI18nMixin(PolymerElement)));


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
  override onBeforeShow(data: DisplaySizeScreenData): void {
    super.onBeforeShow(data);
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
