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
import {mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.js';
import {OobeDialogHostBehavior, OobeDialogHostBehaviorInterface} from '../../components/behaviors/oobe_dialog_host_behavior.js';
import {OobeI18nBehavior, OobeI18nBehaviorInterface} from '../../components/behaviors/oobe_i18n_behavior.js';
import {OobeUiState} from '../../components/display_manager_types.js';

import {getTemplate} from './osauth_error.html.js';

const OsAuthErrorBase = mixinBehaviors(
    [OobeI18nBehavior, OobeDialogHostBehavior, LoginScreenBehavior],
    PolymerElement) as { new (): PolymerElement
      & OobeDialogHostBehaviorInterface
      & OobeI18nBehaviorInterface
      & LoginScreenBehaviorInterface,
    };

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
