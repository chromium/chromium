// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/oobe_icons.html.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/dialogs/oobe_loading_dialog.js';

import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.js';
import {OobeDialogHostBehavior, OobeDialogHostBehaviorInterface} from '../../components/behaviors/oobe_dialog_host_behavior.js';
import {OobeI18nBehavior, OobeI18nBehaviorInterface} from '../../components/behaviors/oobe_i18n_behavior.js';

import {getTemplate} from './apply_online_password.html.js';

const ApplyOnlinePasswordScreenBase = mixinBehaviors(
    [
      OobeDialogHostBehavior,
      OobeI18nBehavior,
      LoginScreenBehavior,
    ],
    PolymerElement) as { new (): PolymerElement
      & OobeDialogHostBehaviorInterface
      & OobeI18nBehaviorInterface
      & LoginScreenBehaviorInterface,
  };

export class ApplyOnlinePasswordScreen extends ApplyOnlinePasswordScreenBase {
  static get is() {
    return 'apply-online-password-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {};
  }

  override ready() {
    super.ready();
    this.initializeLoginScreen('ApplyOnlinePasswordScreen');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [ApplyOnlinePasswordScreen.is]: ApplyOnlinePasswordScreen;
  }
}

customElements.define(ApplyOnlinePasswordScreen.is, ApplyOnlinePasswordScreen);
