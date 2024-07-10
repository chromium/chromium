// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Some of the properties and class names doesn't follow naming convention.
// Disable naming-convention checks.
/* eslint-disable @typescript-eslint/naming-convention */

import '//resources/ash/common/cr_elements/cros_color_overrides.css.js';
import '//resources/ash/common/cr_elements/cr_radio_button/cr_card_radio_button.js';
import '//resources/ash/common/cr_elements/cr_radio_group/cr_radio_group.js';
import '//resources/js/action_link.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/buttons/oobe_back_button.js';
import '../../components/buttons/oobe_next_button.js';
import '../../components/hd_iron_icon.js';
import '../../components/oobe_icons.html.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/cr_card_radio_group_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {OobeModalDialog} from '../../components/dialogs/oobe_modal_dialog.js';
import {OobeUiState} from '../../components/display_manager_types.js';
import {LoginScreenMixin} from '../../components/mixins/login_screen_mixin.js';
import {MultiStepMixin} from '../../components/mixins/multi_step_mixin.js';
import {OobeI18nMixin} from '../../components/mixins/oobe_i18n_mixin.js';
import {Oobe} from '../../cr_ui.js';

import {getTemplate} from './user_creation.html.js';

export interface UserCreation {
  $: {
    learnMoreDialog: OobeModalDialog,
    learnMoreLink: HTMLAnchorElement,
  };
}

export const UserCreationScreenElementBase =
    LoginScreenMixin(MultiStepMixin(OobeI18nMixin(PolymerElement)));

/**
 * UI mode for the dialog.
 */
enum UserCreationUIState {
  CREATE = 'create',
  ENROLL_TRIAGE = 'enroll-triage',
  CHILD_SETUP = 'child-setup',
}

/**
 * User type for setting up the device.
 */
enum UserCreationUserType {
  SELF = 'self',
  CHILD = 'child',
  ENROLL = 'enroll',
}

/**
 * Enroll triage method for setting up the device.
 */
enum EnrollTriageMethod {
  ENROLL = 'enroll',
  SIGNIN = 'signin',
}

/**
 * Available user actions.
 */
enum UserAction {
  SIGNIN = 'signin',
  SIGNIN_TRIAGE = 'signin-triage',
  SIGNIN_SCHOOL = 'signin-school',
  ADD_CHILD = 'add-child',
  ENROLL = 'enroll',
  TRIAGE = 'triage',
  CHILD_SETUP = 'child-setup',
  CANCEL = 'cancel',
}

/**
 * Enroll triage method for setting up the device.
 */
enum ChildSetupMethod {
  CHILD_ACCOUNT = 'child-account',
  SCHOOL_ACCOUNT = 'school-account',
}

export class UserCreation extends UserCreationScreenElementBase {
  static get is() {
    return 'user-creation-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      /**
       * The currently selected user type.
       */
      selectedUserType: {
        type: String,
      },

      /**
       * The currently selected sign in method.
       */
      selectedEnrollTriageMethod: {
        type: String,
      },

      /**
       * The currently selected child setup method.
       */
      selectedChildSetupMethod: {
        type: String,
      },


      /**
       * Is the back button visible on the first step of the screen. Back button
       * is visible iff we are in the add person flow.
       */
      isBackButtonVisible_: {
        type: Boolean,
      },

      titleKey_: {
        type: String,
      },

      subtitleKey_: {
        type: String,
      },

      /**
       * Whether software updaate feature is enabled.
       */
      isOobeSoftwareUpdateEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isOobeSoftwareUpdateEnabled');
        },
        readOnly: true,
      },

      /**
       * Indicates if all OOBE screens have been loaded, so that it's safe to
       * enable the Next and Back buttons.
       */
      isOobeLoaded_: Boolean,
    };
  }

  selectedUserType: string;
  selectedEnrollTriageMethod: string;
  selectedChildSetupMethod: string;
  isBackButtonVisible_: boolean;
  titleKey_: string;
  subtitleKey_: string;
  isOobeLoaded_: boolean;
  private readonly isOobeSoftwareUpdateEnabled_: boolean;

  constructor() {
    super();
    if (this.isOobeSoftwareUpdateEnabled_) {
      this.selectedUserType = '';
      this.titleKey_ = 'userCreationUpdatedTitle';
      this.subtitleKey_ = 'userCreationUpdatedSubtitle';
    } else {
      this.titleKey_ = 'userCreationTitle';
      this.subtitleKey_ = 'userCreationSubtitle';
      this.selectedUserType = UserCreationUserType.SELF;
    }
    this.selectedEnrollTriageMethod = '';
    this.selectedChildSetupMethod = '';
    this.isBackButtonVisible_ = false;
  }

  override get EXTERNAL_API(): string[] {
    return [
      'setIsBackButtonVisible',
      'setTriageStep',
      'setChildSetupStep',
      'setDefaultStep',
    ];
  }

  override defaultUIStep() {
    return UserCreationUIState.CREATE;
  }

  override get UI_STEPS() {
    return UserCreationUIState;
  }

  override onBeforeShow(): void {
    super.onBeforeShow();
    if (this.isOobeSoftwareUpdateEnabled_) {
      this.restoreOobeUIState();
      if (!loadTimeData.getBoolean('isOobeFlow')) {
        this.titleKey_ = 'userCreationAddPersonUpdatedTitle';
        this.subtitleKey_ = 'userCreationAddPersonUpdatedSubtitle';
      } else {
        this.titleKey_ = 'userCreationUpdatedTitle';
        this.subtitleKey_ = 'userCreationUpdatedSubtitle';
      }

      return;
    }

    this.selectedUserType = UserCreationUserType.SELF;
    if (!loadTimeData.getBoolean('isOobeFlow')) {
      this.titleKey_ = 'userCreationAddPersonTitle';
      this.subtitleKey_ = 'userCreationAddPersonSubtitle';
    } else {
      this.titleKey_ = 'userCreationTitle';
      this.subtitleKey_ = 'userCreationSubtitle';
    }
  }

  setDefaultStep(): void {
    Oobe.getInstance().setOobeUiState(OobeUiState.USER_CREATION);
    this.setUIStep(UserCreationUIState.CREATE);
    this.selectedUserType = UserCreationUserType.SELF;
    this.selectedEnrollTriageMethod = '';
    this.selectedChildSetupMethod = '';
  }

  override ready(): void {
    super.ready();
    this.initializeLoginScreen('UserCreationScreen');

    if (loadTimeData.getBoolean('isOobeLazyLoadingEnabled')) {
      // The UserCreation screen is a priority screen, so it becomes visible
      // before the remaining of the OOBE flow is fully loaded. 'Back' and
      // 'Next' buttons are initially disabled, and enabled upon receiving the
      //|oobe-screens-loaded| event.
      this.isOobeLoaded_ = false;
      document.addEventListener('oobe-screens-loaded', () => {
        this.isOobeLoaded_ = true;
      }, {once: true});
    } else {
      this.isOobeLoaded_ = true;
    }
  }

  override getOobeUIInitialState(): OobeUiState {
    return OobeUiState.USER_CREATION;
  }

  // this will allows to restore the oobe UI state
  // ex: click for child -> choose google account -> AddChild Screen is shown
  // clicking back will display user creation screen with child setup step
  // and we need to restore the oobe ui state.
  restoreOobeUIState(): void {
    if (this.uiStep === UserCreationUIState.ENROLL_TRIAGE) {
      Oobe.getInstance().setOobeUiState(OobeUiState.ENROLL_TRIAGE);
    }
    if (this.uiStep === UserCreationUIState.CREATE) {
      Oobe.getInstance().setOobeUiState(OobeUiState.USER_CREATION);
    }
    if (this.uiStep === UserCreationUIState.CHILD_SETUP) {
      Oobe.getInstance().setOobeUiState(OobeUiState.SETUP_CHILD);
    }
  }

  setIsBackButtonVisible(isVisible: boolean): void {
    this.isBackButtonVisible_ = isVisible;
  }

  cancel(): void {
    if (this.isBackButtonVisible_) {
      this.onBackClicked_();
    }
  }

  private onBackClicked_(): void {
    if (this.uiStep === UserCreationUIState.ENROLL_TRIAGE ||
        this.uiStep === UserCreationUIState.CHILD_SETUP) {
      this.setUIStep(UserCreationUIState.CREATE);
      this.selectedUserType = '';
      Oobe.getInstance().setOobeUiState(OobeUiState.USER_CREATION);
    } else {
      this.userActed(UserAction.CANCEL);
    }
  }

  private isNextButtonEnabled_(selection: string, isOobeLoaded: boolean):
      boolean {
    return selection !== '' && isOobeLoaded;
  }

  private onNextClicked_(): void {
    if (this.uiStep === UserCreationUIState.CREATE) {
      if (this.selectedUserType === UserCreationUserType.SELF) {
        this.userActed(UserAction.SIGNIN);
      } else if (this.selectedUserType === UserCreationUserType.CHILD) {
        if (this.isOobeSoftwareUpdateEnabled_) {
          this.userActed(UserAction.CHILD_SETUP);
        } else {
          this.userActed(UserAction.ADD_CHILD);
        }
      } else if (this.selectedUserType === UserCreationUserType.ENROLL) {
        this.userActed(UserAction.TRIAGE);
      }
    }
  }

  private onLearnMoreClicked_(): void {
    this.$.learnMoreDialog.showDialog();
  }

  private focusLearnMoreLink_(): void {
    this.$.learnMoreLink.focus();
  }

  setTriageStep(): void {
    Oobe.getInstance().setOobeUiState(OobeUiState.ENROLL_TRIAGE);
    this.setUIStep(UserCreationUIState.ENROLL_TRIAGE);
  }

  setChildSetupStep(): void {
    this.setUIStep(UserCreationUIState.CHILD_SETUP);
    Oobe.getInstance().setOobeUiState(OobeUiState.SETUP_CHILD);
  }

  private onTriageNextClicked_(): void {
    if (this.selectedEnrollTriageMethod === EnrollTriageMethod.ENROLL) {
      this.userActed(UserAction.ENROLL);
    } else if (this.selectedEnrollTriageMethod === EnrollTriageMethod.SIGNIN) {
      this.userActed(UserAction.SIGNIN_TRIAGE);
    }
  }

  private onChildSetupNextClicked_(): void {
    if (this.selectedChildSetupMethod === ChildSetupMethod.CHILD_ACCOUNT) {
      this.userActed(UserAction.ADD_CHILD);
    } else if (
        this.selectedChildSetupMethod === ChildSetupMethod.SCHOOL_ACCOUNT) {
      this.userActed(UserAction.SIGNIN_SCHOOL);
    }
  }

  private getPersonalCardLabel_(locale: string): string {
    if (this.isOobeSoftwareUpdateEnabled_) {
      return this.i18nDynamic(locale, 'userCreationPersonalButtonTitle');
    }
    return this.i18nDynamic(locale, 'createForSelfLabel');
  }

  private getPersonalCardText_(locale: string): string {
    if (this.isOobeSoftwareUpdateEnabled_) {
      return this.i18nDynamic(locale, 'userCreationPersonalButtonDescription');
    }
    return this.i18nDynamic(locale, 'createForSelfDescription');
  }

  private getChildCardLabel_(locale: string): string {
    if (this.isOobeSoftwareUpdateEnabled_) {
      return this.i18nDynamic(locale, 'userCreationChildButtonTitle');
    }
    return this.i18nDynamic(locale, 'createForChildLabel');
  }

  private getChildCardText_(locale: string): string {
    if (this.isOobeSoftwareUpdateEnabled_) {
      return this.i18nDynamic(locale, 'userCreationChildButtonDescription');
    }
    return this.i18nDynamic(locale, 'createForChildDescription');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [UserCreation.is]: UserCreation;
  }
}

customElements.define(UserCreation.is, UserCreation);
