// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Oobe signin screen implementation.
 */

import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/dialogs/oobe_loading_dialog.js';

import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenMixin} from '../../components/mixins/login_screen_mixin.js';
import {MultiStepMixin} from '../../components/mixins/multi_step_mixin.js';
import {OobeI18nMixin} from '../../components/mixins/oobe_i18n_mixin.js';

import {getTemplate} from './online_authentication_screen.html.js';

/**
 * UI mode for the dialog.
 */
enum DialogMode {
  LOADING = 'loading',
}

const OnlineAuthenticationScreenElementBase =
    LoginScreenMixin(MultiStepMixin(OobeI18nMixin(PolymerElement)));

export class OnlineAuthenticationScreenElement extends OnlineAuthenticationScreenElementBase {
  static get is() {
    return 'online-authentication-screen-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {};
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  override defaultUIStep() {
    return DialogMode.LOADING;
  }

  override get UI_STEPS() {
    return DialogMode;
  }

  override ready() {
    super.ready();
    this.initializeLoginScreen('OnlineAuthenticationScreen');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [OnlineAuthenticationScreenElement.is]: OnlineAuthenticationScreenElement;
  }
}

customElements.define(OnlineAuthenticationScreenElement.is, OnlineAuthenticationScreenElement);
