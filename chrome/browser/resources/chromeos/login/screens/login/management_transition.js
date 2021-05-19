// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design management
 * transition screen.
 */

(function() {

const UIState = {
  PROGRESS: 'progress',
  ERROR: 'error',
};

Polymer({
  is: 'management-transition-element',

  behaviors: [OobeI18nBehavior, LoginScreenBehavior, MultiStepBehavior],

  properties: {
    /**
     * Flag that determines whether management is being removed or added.
     */
    isRemovingManagement_: Boolean,
  },

  UI_STEPS: UIState,

  defaultUIStep() {
    return UIState.PROGRESS;
  },

  ready() {
    this.initializeLoginScreen('ManagementTransitionScreen', {
      resetAllowed: false,
    });
  },

  onBeforeShow(data) {
    this.setIsRemovingManagement(data['isRemovingManagement']);
  },

  setIsRemovingManagement(is_removing_management) {
    this.isRemovingManagement_ = is_removing_management;
  },

  /** @override */
  attached() {
    cr.addWebUIListener(
        'management-transition-failed',
        this.showManagementTransitionFailedScreen_.bind(this));
  },

  /** @private */
  getDialogA11yTitle_(locale, isRemovingManagement) {
    return isRemovingManagement ? this.i18n('removingSupervisionTitle') :
                                  this.i18n('addingSupervisionTitle');
  },

  /** @private */
  showManagementTransitionFailedScreen_() {
    this.setUIStep(UIState.ERROR);
  },

  /**
   * On-tap event handler for OK button.
   *
   * @private
   */
  onAcceptAndContinue_() {
    chrome.send('finishManagementTransition');
  },
});
})();
