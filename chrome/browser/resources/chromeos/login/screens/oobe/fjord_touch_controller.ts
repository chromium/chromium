// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenMixin} from '../../components/mixins/login_screen_mixin.js';
import {OobeDialogHostMixin} from '../../components/mixins/oobe_dialog_host_mixin.js';
import {OobeI18nMixin} from '../../components/mixins/oobe_i18n_mixin.js';

import {getTemplate} from './fjord_touch_controller.html.js';

export const FjordTouchControllerScreenElementBase =
    OobeDialogHostMixin(LoginScreenMixin(OobeI18nMixin(PolymerElement)));

export class FjordTouchControllerScreen extends
    FjordTouchControllerScreenElementBase {
  static get is() {
    return 'fjord-touch-controller-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {};
  }

  override ready(): void {
    super.ready();
    this.initializeLoginScreen('FjordTouchControllerScreen');
  }

  /**
   * Returns the control which should receive initial focus.
   */
  override get defaultControl(): HTMLElement|null {
    return this.shadowRoot!.querySelector('#oobeFrame');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [FjordTouchControllerScreen.is]: FjordTouchControllerScreen;
  }
}

customElements.define(
    FjordTouchControllerScreen.is, FjordTouchControllerScreen);
