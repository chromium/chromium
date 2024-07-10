// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @fileoverview Polymer element for add child screen.
 */


import '//resources/ash/common/cr_elements/cros_color_overrides.css.js';
import '//resources/ash/common/cr_elements/cr_radio_button/cr_card_radio_button.js';
import '//resources/ash/common/cr_elements/cr_radio_group/cr_radio_group.js';
import '//resources/js/action_link.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/hd_iron_icon.js';
import '../../components/oobe_icons.html.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/cr_card_radio_group_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';

import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {OobeModalDialog} from '../../components/dialogs/oobe_modal_dialog.js';
import {OobeUiState} from '../../components/display_manager_types.js';
import {LoginScreenMixin} from '../../components/mixins/login_screen_mixin.js';
import {MultiStepMixin} from '../../components/mixins/multi_step_mixin.js';
import {OobeI18nMixin} from '../../components/mixins/oobe_i18n_mixin.js';

import {getTemplate} from './add_child.html.js';

const AddChildScreenElementBase =
    LoginScreenMixin(MultiStepMixin(OobeI18nMixin(PolymerElement)));

/**
 * Sign in method for setting up the device for child.
 */
enum AddChildSignInMethod {
  CREATE = 'create',
  SIGNIN = 'signin',
}

/**
 * Available user actions.
 */
enum UserAction {
  CREATE = 'child-account-create',
  SIGNIN = 'child-signin',
  BACK = 'child-back',
}

/**
 * UI mode for the dialog.
 */
enum AddChildUiStep {
  OVERVIEW = 'overview',
}

export class AddChildScreen extends AddChildScreenElementBase {
  static get is() {
    return 'add-child-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      /**
       * The currently selected sign in method.
       */
      selectedSignInMethod: {
        type: String,
      },
    };
  }

  private selectedSignInMethod: string;

  constructor() {
    super();
    this.selectedSignInMethod = '';
  }

  override onBeforeShow(): void {
    super.onBeforeShow();
    this.selectedSignInMethod = '';
  }

  override ready(): void {
    super.ready();
    this.initializeLoginScreen('AddChildScreen');
  }

  override get UI_STEPS() {
    return AddChildUiStep;
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  override defaultUIStep() {
    return AddChildUiStep.OVERVIEW;
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  override getOobeUIInitialState() {
    return OobeUiState.GAIA_SIGNIN;
  }

  private cancel(): void {
    this.onBackClicked_();
  }

  private onBackClicked_(): void {
    this.userActed(UserAction.BACK);
  }

  private onNextClicked_(): void {
    if (this.selectedSignInMethod === AddChildSignInMethod.CREATE) {
      this.userActed(UserAction.CREATE);
    } else if (this.selectedSignInMethod === AddChildSignInMethod.SIGNIN) {
      this.userActed(UserAction.SIGNIN);
    }
  }

  private onLearnMoreClicked_(): void {
    this.shadowRoot!.querySelector<OobeModalDialog>('#learnMoreDialog')!
      .showDialog();
  }

  private focusLearnMoreLink_(): void {
    this.shadowRoot!.querySelector<HTMLAnchorElement>('#learnMoreLink')!
      .focus();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [AddChildScreen.is]: AddChildScreen;
  }
}

customElements.define(AddChildScreen.is, AddChildScreen);
