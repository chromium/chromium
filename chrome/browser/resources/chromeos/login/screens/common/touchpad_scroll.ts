// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @fileoverview Polymer element for touchpad scroll screen.
 */

import '//resources/ash/common/cr_elements/cros_color_overrides.css.js';
import '//resources/polymer/v3_0/iron-iconset-svg/iron-iconset-svg.js';
import '../../components/buttons/oobe_next_button.js';
import '../../components/buttons/oobe_text_button.js';
import '../../components/oobe_icons.html.js';
import '../../components/oobe_illo_icons.html.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';

import {assert} from '//resources/js/assert.js';
import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.js';
import {MultiStepBehavior, MultiStepBehaviorInterface} from '../../components/behaviors/multi_step_behavior.js';
import {OobeI18nBehavior, OobeI18nBehaviorInterface} from '../../components/behaviors/oobe_i18n_behavior.js';
import {OobeUiState} from '../../components/display_manager_types.js';

import {getTemplate} from './touchpad_scroll.html.js';

const TouchpadScrollScreenElementBase =
    mixinBehaviors(
        [OobeI18nBehavior, LoginScreenBehavior, MultiStepBehavior],
        PolymerElement) as {
      new (): PolymerElement & OobeI18nBehaviorInterface &
          LoginScreenBehaviorInterface & MultiStepBehaviorInterface,
    };

/**
 * Data that is passed to the screen during onBeforeShow.
 */
interface TouchpadScrollScreenData {
  shouldShowReturn: boolean;
}

/**
 * Enum to represent steps on the touchpad scroll screen.
 * Currently there is only one step, but we still use
 * MultiStepBehavior because it provides implementation of
 * things like processing 'focus-on-show' class
 */
enum TouchpadScrollStep {
  OVERVIEW = 'overview',
}

/**
 * Available user actions.
 */
enum UserAction {
  NEXT = 'next',
  REVERSE = 'update-scroll',
  RETURN = 'return',
}

export class TouchpadScrollScreen extends TouchpadScrollScreenElementBase {
  static get is() {
    return 'touchpad-scroll-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      isReverseScrolling: {
        type: Boolean,
        value: false,
        observer: 'onCheckChanged',
      },

      /**
       * Whether the button to return to CHOOBE screen should be shown.
       */
      shouldShowReturn: {
        type: Boolean,
        value: false,
      },
    };
  }

  private isReverseScrolling: boolean;
  private shouldShowReturn: boolean;
  private resizeobserver: ResizeObserver;

  constructor() {
    super();
    this.resizeobserver = new ResizeObserver(() => this.onScrollAreaResize());
  }

  override get EXTERNAL_API(): string[] {
    return ['setReverseScrolling'];
  }

  override get UI_STEPS() {
    return TouchpadScrollStep;
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  override defaultUIStep(): TouchpadScrollStep {
    return TouchpadScrollStep.OVERVIEW;
  }

  override ready(): void {
    super.ready();
    this.initializeLoginScreen('TouchpadScrollScreen');
    const scrollArea =
        this.shadowRoot?.querySelector<HTMLElement>('#scrollArea');
    if (scrollArea instanceof HTMLElement) {
      this.resizeobserver.observe(scrollArea);
    }
  }

  onScrollAreaResize(): void {
    const scrollArea = this.shadowRoot?.querySelector('#scrollArea');
    assert(scrollArea instanceof HTMLElement);
    // Removing the margin to set it
    scrollArea.scrollTop = scrollArea.scrollHeight / 2 - 150;
  }

  onBeforeShow(data: TouchpadScrollScreenData): void {
    this.shouldShowReturn = data['shouldShowReturn'];
  }

  /**
   * Set the toggle to the synced
   * scrolling preferences.
   */
  setReverseScrolling(isReverseScrolling: boolean): void {
    this.isReverseScrolling = isReverseScrolling;
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  override getOobeUIInitialState(): OobeUiState {
    return OobeUiState.CHOOBE;
  }

  private onCheckChanged(newValue: boolean, oldValue: boolean): void {
    // Do not forward action to browser during property initialization
    if (oldValue != null) {
      this.userActed([UserAction.REVERSE, newValue]);
    }
  }

  private onNextClicked(): void {
    this.userActed(UserAction.NEXT);
  }

  private onReturnClicked(): void {
    this.userActed(UserAction.RETURN);
  }

  private getAriaLabelToggleButtons(
      locale: string, title: string, subtitle: string): string {
    return this.i18nDynamic(locale, title) + '. ' +
        this.i18nDynamic(locale, subtitle);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [TouchpadScrollScreen.is]: TouchpadScrollScreen;
  }
}

customElements.define(TouchpadScrollScreen.is, TouchpadScrollScreen);
