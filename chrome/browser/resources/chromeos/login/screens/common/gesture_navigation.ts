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

import {assert} from '//resources/js/assert.js';
import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {OobeAdaptiveDialog} from '../../components/dialogs/oobe_adaptive_dialog.js';
import {LoginScreenMixin} from '../../components/mixins/login_screen_mixin.js';
import {MultiStepMixin} from '../../components/mixins/multi_step_mixin.js';
import {OobeI18nMixin} from '../../components/mixins/oobe_i18n_mixin.js';
import type {OobeCrLottie} from '../../components/oobe_cr_lottie.js';
import {GestureNavigationPageHandler_GesturePages, GestureNavigationPageHandlerRemote} from '../../mojom-webui/screens_common.mojom-webui.js';
import {OobeScreensFactoryBrowserProxy} from '../../oobe_screens_factory_proxy.js';

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

export const GestureScreenElementBase =
    LoginScreenMixin(MultiStepMixin(OobeI18nMixin(PolymerElement)));

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

  private handler: GestureNavigationPageHandlerRemote;

  constructor() {
    super();
    this.handler = new GestureNavigationPageHandlerRemote();
    OobeScreensFactoryBrowserProxy.getInstance()
        .screenFactory.establishGestureNavigationScreenPipe(
            this.handler.$.bindNewPipeAndPassReceiver());
  }

  override get UI_STEPS() {
    return GesturePage;
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
    this.handler.onSkipClicked();
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
        this.handler.onExitClicked();
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
    this.setPlayCurrentScreenAnimation(true);
    switch (this.uiStep) {
      case GesturePage.INTRO:
        this.handler.onPageChange(
            GestureNavigationPageHandler_GesturePages.kIntro);
        break;
      case GesturePage.HOME:
        this.handler.onPageChange(
            GestureNavigationPageHandler_GesturePages.kHome);
        break;
      case GesturePage.OVERVIEW:
        this.handler.onPageChange(
            GestureNavigationPageHandler_GesturePages.kOverview);
        break;
      case GesturePage.BACK:
        this.handler.onPageChange(
            GestureNavigationPageHandler_GesturePages.kBack);
        break;
    }
  }

  /**
   * This will play or stop the current screen's lottie animation.
   * param enabled Whether the animation should play or not.
   */
  private setPlayCurrentScreenAnimation(enabled: boolean): void {
    const currentStep =
        this.shadowRoot?.querySelector<OobeAdaptiveDialog>('#' + this.uiStep);
    assert(currentStep instanceof OobeAdaptiveDialog);
    const animation =
        currentStep.querySelector<OobeCrLottie>('.gesture-animation');
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
