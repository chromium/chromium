// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for Parental Handoff screen.
 */

import '//resources/ash/common/cr_elements/cr_shared_vars.css.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/oobe_icons.html.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';

import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {OobeAdaptiveDialog} from '../../components/dialogs/oobe_adaptive_dialog.js';
import {LoginScreenMixin} from '../../components/mixins/login_screen_mixin.js';
import {OobeDialogHostMixin} from '../../components/mixins/oobe_dialog_host_mixin.js';
import {OobeI18nMixin} from '../../components/mixins/oobe_i18n_mixin.js';

import {getTemplate} from './parental_handoff.html.js';

export const ParentalHandoffElementBase =
    OobeDialogHostMixin(LoginScreenMixin(OobeI18nMixin(PolymerElement)));

interface ParentalHandoffScreenData {
  username: string;
}

export class ParentalHandoff extends ParentalHandoffElementBase {
  static get is() {
    return 'parental-handoff-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      /**
       * The username to be displayed
       */
      username: {
        type: String,
        value: '',
      },
    };
  }

  private username: string;

  constructor() {
    super();
  }

  /**
   * Event handler that is invoked just before the frame is shown.
   */
  override onBeforeShow(data: ParentalHandoffScreenData): void {
    super.onBeforeShow(data);
    if ('username' in data) {
      this.username = data.username;
    }
    const parentalHandoffDialog =
        this.shadowRoot!.querySelector<OobeAdaptiveDialog>(
            '#parentalHandoffDialog')!;
    parentalHandoffDialog.focus();
  }

  override ready(): void {
    super.ready();
    this.initializeLoginScreen('ParentalHandoffScreen');
  }

  /*
   * Executed on language change.
   */
  override updateLocalizedContent(): void {
    this.i18nUpdateLocale();
  }

  /**
   * On-tap event handler for Next button.
   *
   */
  private onNextButtonPressed(): void {
    this.userActed('next');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [ParentalHandoff.is]: ParentalHandoff;
  }
}

customElements.define(ParentalHandoff.is, ParentalHandoff);
