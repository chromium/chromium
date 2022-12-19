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

import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.js';
import {MultiStepBehavior, MultiStepBehaviorInterface} from '../../components/behaviors/multi_step_behavior.js';
import {OobeI18nBehavior, OobeI18nBehaviorInterface} from '../../components/behaviors/oobe_i18n_behavior.js';


/**
 * Enum to represent each page in the gesture navigation screen.
 * @enum {string}
 */
const GesturePage = {
  INTRO: 'gestureIntro',
  HOME: 'gestureHome',
  OVERVIEW: 'gestureOverview',
  BACK: 'gestureBack',
};

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {LoginScreenBehaviorInterface}
 * @implements {MultiStepBehaviorInterface}
 */
const GestureScreenElementBase = mixinBehaviors(
    [OobeI18nBehavior, LoginScreenBehavior, MultiStepBehavior], PolymerElement);

class GestureNavigation extends GestureScreenElementBase {
  static get is() {
    return 'gesture-navigation-element';
  }

  static get template() {
    return html`{__html_template__}`;
  }


  static get properties() {
    return {};
  }

  constructor() {
    super();
    this.UI_STEPS = GesturePage;
  }

  /** @override */
  get EXTERNAL_API() {
    return [];
  }

  /** @override */
  defaultUIStep() {
    return GesturePage.INTRO;
  }

  /** @override */
  ready() {
    super.ready();
    this.initializeLoginScreen('GestureNavigationScreen');
  }

  /**
   * This is the 'on-tap' event handler for the 'next' or 'get started' button.
   * @private
   */
  onNext_() {
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
        this.userActed('exit');
        break;
    }
  }

  /**
   * This is the 'on-tap' event handler for the 'back' button.
   * @private
   */
  onBack_() {
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
   * @param {GesturePage} newPage The target page.
   * @private
   */
  setCurrentPage_(newPage) {
    this.setPlayCurrentScreenAnimation(false);
    this.setUIStep(newPage);
    this.userActed(['gesture-page-change', newPage]);
    this.setPlayCurrentScreenAnimation(true);
  }

  /**
   * This will play or stop the current screen's lottie animation.
   * @param {boolean} enabled Whether the animation should play or not.
   * @private
   */
  setPlayCurrentScreenAnimation(enabled) {
    var animation = this.$[this.uiStep].querySelector('.gesture-animation');
    if (animation) {
      animation.playing = enabled;
    }
  }
}

customElements.define(GestureNavigation.is, GestureNavigation);
