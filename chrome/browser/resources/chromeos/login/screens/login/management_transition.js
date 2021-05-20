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

/**
 * Possible transition types. Must be in the same order as
 * ArcSupervisionTransition enum values.
 * @enum {number}
 */
const ARC_SUPERVISION_TRANSITION = {
  NO_TRANSITION: 0,
  CHILD_TO_REGULAR: 1,
  REGULAR_TO_CHILD: 2,
  UNMANAGED_TO_MANAGED: 3,
};

Polymer({
  is: 'management-transition-element',

  behaviors: [OobeI18nBehavior, LoginScreenBehavior, MultiStepBehavior],

  properties: {
    /**
     * Flag that determines whether management is being removed or added.
     */
    isRemovingManagement_: Boolean,
    /**
     * Flag that determines whether this is enterprise management or child
     * supervision transition.
     */
    isChildTransition_: Boolean,
    /**
     * String that represents management entity for the user. Can be domain or
     * admin name.
     */
    managementEntity_: String,
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
    this.setArcTransition(data['arcTransition']);
    this.setManagementEntity(data['managementEntity']);
  },

  setArcTransition(arc_transition) {
    switch (arc_transition) {
      case ARC_SUPERVISION_TRANSITION.CHILD_TO_REGULAR:
        this.isChildTransition_ = true;
        this.isRemovingManagement_ = true;
        break;
      case ARC_SUPERVISION_TRANSITION.REGULAR_TO_CHILD:
        this.isChildTransition_ = true;
        this.isRemovingManagement_ = false;
        break;
      case ARC_SUPERVISION_TRANSITION.UNMANAGED_TO_MANAGED:
        this.isChildTransition_ = false;
        this.isRemovingManagement_ = false;
        break;
      case ARC_SUPERVISION_TRANSITION.NO_TRANSITION:
        console.error(
            'Screen should not appear for ARC_SUPERIVISION_TRANSITION.NO_TRANSITION');
        break;
      default:
        console.error('Not handled transition type: ' + arc_transition);
    }
  },

  setManagementEntity(management_entity) {
    this.managementEntity_ = management_entity;
  },

  /** @override */
  attached() {
    cr.addWebUIListener(
        'management-transition-failed',
        this.showManagementTransitionFailedScreen_.bind(this));
  },

  /** @private */
  getDialogTitle_(
      locale, isRemovingManagement, isChildTransition, managementEntity) {
    if (isChildTransition) {
      return isRemovingManagement ? this.i18n('removingSupervisionTitle') :
                                    this.i18n('addingSupervisionTitle');
    } else if (managementEntity) {
      return this.i18n('addingManagementTitle', managementEntity);
    } else {
      return this.i18n('addingManagementTitleUnknownAdmin');
    }
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
