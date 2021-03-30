// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

(function() {

/**
 * UI mode for the dialog.
 * @enum {string}
 */
const UIState = {
  CREATE: 'create',
  CHILD: 'child',
};

/**
 * User type for setting up the device.
 * @enum {string}
 */
const UserType = {
  SELF: 'self',
  CHILD: 'child',
};

/**
 * Sign in method for setting up the device for child.
 * @enum {string}
 */
const SignInMethod = {
  CREATE: 'create',
  SIGNIN: 'signin',
};

Polymer({
  is: 'user-creation-element',

  behaviors: [
    OobeI18nBehavior,
    OobeDialogHostBehavior,
    LoginScreenBehavior,
    MultiStepBehavior,
  ],

  properties: {

    /**
     * The currently selected user type.
     */
    selectedUserType: {
      type: String,
      value: UserType.SELF,
    },

    /**
     * The currently selected sign in method.
     */
    selectedSignInMethod: {
      type: String,
      value: '',
    },

    /**
     * Is the back button visible on the first step of the screen. Back button
     * is visible iff we are in the add person flow.
     */
    isBackButtonVisible_: {
      type: Boolean,
      value: true,
    },

    titleKey_: {
      type: String,
      value: 'userCreationTitle',
    },

    subtitleKey_: {
      type: String,
      value: 'userCreationSubtitle',
    },

  },

  EXTERNAL_API: ['setIsBackButtonVisible'],

  defaultUIStep() {
    return UIState.CREATE;
  },

  UI_STEPS: UIState,

  onBeforeShow() {
    this.selectedUserType = UserType.SELF;
    this.selectedSignInMethod = '';
    this.titleKey_ = this.isBackButtonVisible_ ? 'userCreationAddPersonTitle' :
                                                 'userCreationTitle';
    this.subtitleKey_ = this.isBackButtonVisible_ ?
        'userCreationAddPersonSubtitle' :
        'userCreationSubtitle';
    if (this.uiStep === UIState.CHILD) {
      chrome.send('updateOobeUIState', [OOBE_UI_STATE.GAIA_SIGNIN]);
    }
  },

  ready() {
    this.initializeLoginScreen('UserCreationScreen', {
      resetAllowed: true,
    });
  },

  getOobeUIInitialState() {
    return OOBE_UI_STATE.USER_CREATION;
  },

  setIsBackButtonVisible(isVisible) {
    this.isBackButtonVisible_ = isVisible;
  },

  cancel() {
    if (this.isBackButtonVisible_) {
      this.onBackClicked_();
    }
  },

  onBackClicked_() {
    if (this.uiStep === UIState.CHILD) {
      chrome.send('updateOobeUIState', [OOBE_UI_STATE.USER_CREATION]);
      this.setUIStep(UIState.CREATE);
    } else {
      this.userActed('cancel');
    }
  },

  onNextClicked_() {
    if (this.uiStep === UIState.CREATE) {
      if (this.selectedUserType === UserType.SELF) {
        this.userActed('signin');
      } else if (this.selectedUserType === UserType.CHILD) {
        chrome.send('updateOobeUIState', [OOBE_UI_STATE.GAIA_SIGNIN]);
        this.setUIStep(UIState.CHILD);
      }
    } else if (this.uiStep === UIState.CHILD) {
      if (this.selectedSignInMethod === SignInMethod.CREATE) {
        this.userActed('child-account-create');
      } else if (this.selectedSignInMethod === SignInMethod.SIGNIN) {
        this.userActed('child-signin');
      }
    }
  },

  onLearnMoreClicked_() {
    this.$.learnMoreDialog.showDialog();
  },

  focusLearnMoreLink_() {
    this.$.learnMoreLink.focus();
  },
});
})();
