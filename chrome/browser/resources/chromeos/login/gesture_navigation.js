// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {

/**
 * Enum to represent each page in the gesture navigation screen.
 * @enum {string}
 */
const GesturePage = {
  INTRO: 'gestureIntro',
  HOME: 'gestureHome',
  OVERVIEW: 'gestureOverview',
  BACK: 'gestureBack'
};

Polymer({
  is: 'gesture-navigation-element',

  behaviors: [
    OobeI18nBehavior,
    LoginScreenBehavior,
    MultiStepBehavior
  ],

  UI_STEPS: GesturePage,

  defaultUIStep() {
    return GesturePage.INTRO;
  },

  /** @override */
  ready() {
    this.initializeLoginScreen('GestureNavigationScreen', {
      commonScreenSize: true,
      resetAllowed: true,
    });
  },

  /**
   * This is the 'on-tap' event handler for the 'next' or 'get started' button.
   * @private
   *
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
  },

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
  },

  /**
   * Set the new page, making sure to stop the animation for the old page and
   * start the animation for the new page.
   * @param {GesturePage} newPage The target page.
   * @private
   */
  setCurrentPage_(newPage) {
    this.setPlayCurrentScreenAnimation(false);
    this.setUIStep(newPage);
    chrome.send('handleGesturePageChange', [newPage]);
    this.setPlayCurrentScreenAnimation(true);
  },

  /**
   * This will play or stop the current screen's lottie animation.
   * @param {boolean} enabled Whether the animation should play or not.
   * @private
   */
  setPlayCurrentScreenAnimation(enabled) {
    var animation = this.$[this.uiStep].querySelector('.gesture-animation');
    if (animation) {
      animation.setPlay(enabled);
    }
  },
});
})();
