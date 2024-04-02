// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/ash/common/cr_elements/cros_color_overrides.css.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/oobe_illo_icons.html.js';
import '../../components/buttons/oobe_back_button.js';
import '../../components/buttons/oobe_next_button.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import {OobeAdaptiveDialog} from '../../components/dialogs/oobe_adaptive_dialog.js';

import {assertInstanceof} from '//resources/js/assert.js';
import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.js';
import {OobeDialogHostBehavior, OobeDialogHostBehaviorInterface} from '../../components/behaviors/oobe_dialog_host_behavior.js';
import {OobeI18nMixin, OobeI18nMixinInterface} from '../../components/mixins/oobe_i18n_mixin.js';

import {getTemplate} from './tuna.html.js';

export const TunaScreenElementBase =
    mixinBehaviors(
        [OobeDialogHostBehavior, LoginScreenBehavior],
        OobeI18nMixin(PolymerElement)) as {
      new (): PolymerElement & OobeI18nMixinInterface &
        OobeDialogHostBehaviorInterface & LoginScreenBehaviorInterface,
    };

enum UserAction {
  BACK = 'back',
  NEXT = 'next',
}

/**
 * Data that is passed to the screen during onBeforeShow.
 */
interface TunaScreenData {
  backButtonVisible: boolean;
}

export class TunaScreen extends TunaScreenElementBase {
  static get is() {
    return 'tuna-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      backButtonVisible: {
        type: Boolean,
        value: false,
      },
    };
  }

  private backButtonVisible: boolean;

  override ready(): void {
    super.ready();
    this.initializeLoginScreen('TunaScreen');
  }

  override get defaultControl(): HTMLElement {
    const dialog =  this.shadowRoot?.querySelector('#tunaDialog');
    assertInstanceof(dialog, OobeAdaptiveDialog);
    return dialog;
  }

  override onBeforeShow(data: TunaScreenData): void {
    this.backButtonVisible = data['backButtonVisible'];
  }

  private onBackClicked(): void {
    this.userActed(UserAction.BACK);
  }

  private onNextClicked(): void {
    this.userActed(UserAction.NEXT);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [TunaScreen.is]: TunaScreen;
  }
}

customElements.define(TunaScreen.is, TunaScreen);
