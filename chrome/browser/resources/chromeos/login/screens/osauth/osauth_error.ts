// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for signin fatal error.
 */

import '//resources/js/action_link.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/oobe_icons.html.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';
import '../../components/buttons/oobe_text_button.js';

import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {OobeUiState} from '../../components/display_manager_types.js';
import {LoginScreenMixin} from '../../components/mixins/login_screen_mixin.js';
import {OobeDialogHostMixin} from '../../components/mixins/oobe_dialog_host_mixin.js';
import {OobeI18nMixin} from '../../components/mixins/oobe_i18n_mixin.js';

import {getTemplate} from './osauth_error.html.js';

const OsAuthErrorBase =
    OobeDialogHostMixin(LoginScreenMixin(OobeI18nMixin(PolymerElement)));

export class OsAuthErrorScreen extends OsAuthErrorBase {
  static get is() {
    return 'osauth-error-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }


  static get properties(): PolymerElementProperties {
    return {};
  }

  override ready() {
    super.ready();
    this.initializeLoginScreen('OSAuthErrorScreen');
  }

  /** Initial UI State for screen */
  // eslint-disable-next-line @typescript-eslint/naming-convention
  override getOobeUIInitialState(): OobeUiState {
    return OobeUiState.BLOCKING;
  }

  private onRetryLoginButtonPressed(): void {
    this.userActed('cancelLoginFlow');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [OsAuthErrorScreen.is]: OsAuthErrorScreen;
  }
}

customElements.define(OsAuthErrorScreen.is, OsAuthErrorScreen);
