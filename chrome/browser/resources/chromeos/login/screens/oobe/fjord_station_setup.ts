// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenMixin} from '../../components/mixins/login_screen_mixin.js';
import {OobeDialogHostMixin} from '../../components/mixins/oobe_dialog_host_mixin.js';
import {OobeI18nMixin} from '../../components/mixins/oobe_i18n_mixin.js';

import {getTemplate} from './fjord_station_setup.html.js';

export const FjordStationSetupScreenElementBase =
    OobeDialogHostMixin(LoginScreenMixin(OobeI18nMixin(PolymerElement)));

export class FjordStationSetupScreen extends
    FjordStationSetupScreenElementBase {
  static get is() {
    return 'fjord-station-setup-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {};
  }

  override ready(): void {
    super.ready();
    this.initializeLoginScreen('FjordStationSetupScreen');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [FjordStationSetupScreen.is]: FjordStationSetupScreen;
  }
}

customElements.define(FjordStationSetupScreen.is, FjordStationSetupScreen);
