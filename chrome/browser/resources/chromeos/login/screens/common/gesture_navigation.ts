// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/oobe_cr_lottie.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';
import '../../components/buttons/oobe_back_button.js';
import '../../components/buttons/oobe_next_button.js';
import '../../components/buttons/oobe_text_button.js';
import '../../components/oobe_cr_lottie.js';

import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.js';
import {MultiStepBehavior, MultiStepBehaviorInterface} from '../../components/behaviors/multi_step_behavior.js';
import {OobeI18nBehavior, OobeI18nBehaviorInterface} from '../../components/behaviors/oobe_i18n_behavior.js';
import type {OobeCrLottie} from '../../components/oobe_cr_lottie.js';

import {getTemplate} from './gesture_navigation.html.js';


/**
 * Enum to represent each page in the gesture navigation screen.
 */
enum GesturePage {
  INTRO = 'gestureIntro',
  HOME = 'gestureHome',
  OVERVIEW = 'gestureOverview',
  BACK = 'gestureBack',
}

/**
 * Available user actions.
 */
enum UserAction {
  SKIP = 'skip',
  EXIT = 'exit',
  PAGE_CHANGE = 'gesture-page-change',
}


export const GestureScreenElementBase =
    mixinBehaviors(
        [OobeI18nBehavior, LoginScreenBehavior, MultiStepBehavior],
        PolymerElement) as {
      new (): PolymerElement & OobeI18nBehaviorInterface &
          LoginScreenBehaviorInterface & MultiStepBehaviorInterface,
    };

export class GestureNavigation extends GestureScreenElementBase {
  static get is() {
    return 'gesture-navigation-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }


  static get properties(): PolymerElementProperties {
    return {};
  }

  constructor() {
    super();
  }

  override get UI_STEPS() {
    return GesturePage;
  }


  override get EXTERNAL_API(): string[] {
    return [];
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  override defaultUIStep(): GesturePage {
    return GesturePage.INTRO;
  }

  override ready(): void {
    super.ready();
    this.initializeLoginScreen('GestureNavigationScreen');
  }

  /**
   * This is the 'on-tap' event handler for the skip button.
   */
  private onSkip_(): void {
    this.userActed(UserAction.SKIP);
  }

  /**
   * This is the 'on-tap' event handler for the 'next' or 'get started' button.
   */
  private onNext_(): void {
    switch (this.uiStep) {
      case GesturePage.INTRO:
        this.setCurrentPage_(GesturePage.HOME);
        break;
      case GesturePage.HOME:
        this.setCurrentPage_(GesturePage.OVERVIEW);
        break;
      case GesturePage.OVERVIEW:
        this.setCurrentPage_(GesturePage.BACK);
        break;
      case GesturePage.BACK:
        // Exiting the last page in the sequence - stop the animation, and
        // report exit. Keep the currentPage_ value so the UI does not get
        // updated until the next screen is shown.
        this.setPlayCurrentScreenAnimation(false);
        this.userActed(UserAction.EXIT);
        break;
    }
  }

  /**
   * This is the 'on-tap' event handler for the 'back' button.
   */
  private onBack_(): void {
    switch (this.uiStep) {
      case GesturePage.HOME:
        this.setCurrentPage_(GesturePage.INTRO);
        break;
      case GesturePage.OVERVIEW:
        this.setCurrentPage_(GesturePage.HOME);
        break;
      case GesturePage.BACK:
        this.setCurrentPage_(GesturePage.OVERVIEW);
        break;
    }
  }

  /**
   * Set the new page, making sure to stop the animation for the old page and
   * start the animation for the new page.
   */
  private setCurrentPage_(newPage: GesturePage): void {
    this.setPlayCurrentScreenAnimation(false);
    this.setUIStep(newPage);
    this.userActed([UserAction.PAGE_CHANGE, newPage]);
    this.setPlayCurrentScreenAnimation(true);
  }

  /**
   * This will play or stop the current screen's lottie animation.
   * param enabled Whether the animation should play or not.
   */
  private setPlayCurrentScreenAnimation(enabled: boolean): void {
    const animation =
        this.shadowRoot!.querySelector<OobeCrLottie>('.gesture-animation');
    if (animation) {
      animation.playing = enabled;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [GestureNavigation.is]: GestureNavigation;
  }
}

customElements.define(GestureNavigation.is, GestureNavigation);
