// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design supervision
 * transition screen.
 */

(function() {

const UIState = {
  PROGRESS: 'progress',
  ERROR: 'error',
};

Polymer({
  is: 'supervision-transition-element',

  behaviors: [
    OobeI18nBehavior,
    LoginScreenBehavior,
    MultiStepBehavior
  ],

  properties: {
    /**
     * Flag that determines whether supervision is being removed or added.
     */
    isRemovingSupervision_: Boolean,
  },

  UI_STEPS: UIState,

  defaultUIStep() {
    return UIState.PROGRESS;
  },

  ready() {
    this.initializeLoginScreen('SupervisionTransitionScreen', {
      resetAllowed: false,
    });
  },

  onBeforeShow(data) {
    this.setIsRemovingSupervision(data['isRemovingSupervision']);
  },

  setIsRemovingSupervision(is_removing_supervision) {
    this.isRemovingSupervision_ = is_removing_supervision;
  },

  /** @override */
  attached() {
    cr.addWebUIListener(
        'supervision-transition-failed',
        this.showSupervisionTransitionFailedScreen_.bind(this));
  },

  /** @private */
  getDialogA11yTitle_(locale, isRemovingSupervision) {
    return isRemovingSupervision ? this.i18n('removingSupervisionTitle') :
                                   this.i18n('addingSupervisionTitle');
  },

  /** @private */
  showSupervisionTransitionFailedScreen_() {
    this.setUIStep(UIState.ERROR);
  },

  /**
   * On-tap event handler for OK button.
   *
   * @private
   */
  onAcceptAndContinue_() {
    chrome.send('finishSupervisionTransition');
  },
});
})();
