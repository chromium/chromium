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

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {OobeUiState} from '../../components/display_manager_types.js';
import {LoginScreenMixin} from '../../components/mixins/login_screen_mixin.js';
import {OobeDialogHostMixin} from '../../components/mixins/oobe_dialog_host_mixin.js';
import {OobeI18nMixin} from '../../components/mixins/oobe_i18n_mixin.js';

import {getTemplate} from './wrong_hwid.html.js';

const WrongHwIdBase =
    OobeDialogHostMixin(LoginScreenMixin(OobeI18nMixin(PolymerElement)));

export class WrongHwId extends WrongHwIdBase {
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
    [WrongHwId.is]: WrongHwId;
  }
}

customElements.define(WrongHwId.is, WrongHwId);
