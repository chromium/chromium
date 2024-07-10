// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for OS trial screen.
 */

import '//resources/ash/common/cr_elements/cros_color_overrides.css.js';
import '//resources/ash/common/cr_elements/cr_radio_button/cr_radio_button.js';
import '//resources/ash/common/cr_elements/cr_radio_group/cr_radio_group.js';
import '//resources/ash/common/cr_elements/cr_shared_vars.css.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/oobe_icons.html.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/cr_card_radio_group_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';
import '../../components/hd_iron_icon.js';
import '../../components/buttons/oobe_back_button.js';
import '../../components/buttons/oobe_next_button.js';

import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenMixin} from '../../components/mixins/login_screen_mixin.js';
import {OobeDialogHostMixin} from '../../components/mixins/oobe_dialog_host_mixin.js';
import {OobeI18nMixin} from '../../components/mixins/oobe_i18n_mixin.js';

import {getTemplate} from './os_trial.html.js';


/**
 * Trial option for setting up the device.
 */
enum TrialOption {
  INSTALL = 'install',
  TRY = 'try',
}

const OsTrialScreenElementBase =
    OobeDialogHostMixin(LoginScreenMixin(OobeI18nMixin(PolymerElement)));

export class OsTrial extends OsTrialScreenElementBase {
  static get is() {
    return 'os-trial-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      /**
       * The currently selected trial option.
       */
      selectedTrialOption: {
        type: String,
        value: TrialOption.INSTALL,
      },
    };
  }

  private selectedTrialOption: TrialOption;

  constructor() {
    super();
  }

  override ready(): void {
    super.ready();
    this.initializeLoginScreen('OsTrialScreen');
  }

  /**
   * This is the 'on-click' event handler for the 'next' button.
   */
  private onNextButtonClick(): void {
    if (this.selectedTrialOption === TrialOption.TRY) {
      this.userActed('os-trial-try');
    } else {
      this.userActed('os-trial-install');
    }
  }

  /**
   * This is the 'on-click' event handler for the 'back' button.
   */
  private onBackButtonClick(): void {
    this.userActed('os-trial-back');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [OsTrial.is]: OsTrial;
  }
}

customElements.define(OsTrial.is, OsTrial);
