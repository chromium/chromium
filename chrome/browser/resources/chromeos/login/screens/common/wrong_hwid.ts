// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview wrong HWID screen implementation.
 */

import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/oobe_icons.html.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';
import '../../components/buttons/oobe_text_button.js';

import {mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.js';
import {OobeDialogHostBehavior, OobeDialogHostBehaviorInterface} from '../../components/behaviors/oobe_dialog_host_behavior.js';
import {OobeI18nBehavior, OobeI18nBehaviorInterface} from '../../components/behaviors/oobe_i18n_behavior.js';
import {OobeUiState} from '../../components/display_manager_types.js';

import {getTemplate} from './wrong_hwid.html.js';

const WronHwIdBase =
    mixinBehaviors(
        [OobeI18nBehavior, OobeDialogHostBehavior, LoginScreenBehavior],
        PolymerElement) as {
      new (): PolymerElement & OobeI18nBehaviorInterface &
          OobeDialogHostBehaviorInterface & LoginScreenBehaviorInterface,
    };

export class WronHwId extends WronHwIdBase {
  static get is() {
    return 'wrong-hwid-element' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {};
  }

  override ready() {
    super.ready();
    this.initializeLoginScreen('WrongHWIDMessageScreen');
  }

  /** Initial UI State for screen */
  // eslint-disable-next-line @typescript-eslint/naming-convention
  override getOobeUIInitialState(): OobeUiState {
    return OobeUiState.WRONG_HWID_WARNING;
  }

  onSkip() {
    this.userActed('skip-screen');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [WronHwId.is]: WronHwId;
  }
}

customElements.define(WronHwId.is, WronHwId);
