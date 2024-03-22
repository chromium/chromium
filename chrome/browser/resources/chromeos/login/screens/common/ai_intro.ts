// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/ash/common/cr_elements/cros_color_overrides.css.js';
import '../../components/buttons/oobe_next_button.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import {OobeAdaptiveDialog} from '../../components/dialogs/oobe_adaptive_dialog.js';

import {assertInstanceof} from '//resources/js/assert.js';
import {mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.js';
import {OobeDialogHostBehavior, OobeDialogHostBehaviorInterface} from '../../components/behaviors/oobe_dialog_host_behavior.js';
import {OobeI18nMixin, OobeI18nMixinInterface} from '../../components/mixins/oobe_i18n_mixin.js';

import {getTemplate} from './ai_intro.html.js';

export const AiIntroScreenElementBase =
    mixinBehaviors(
        [OobeDialogHostBehavior, LoginScreenBehavior],
        OobeI18nMixin(PolymerElement)) as {
          new (): PolymerElement & OobeI18nMixinInterface &
            OobeDialogHostBehaviorInterface & LoginScreenBehaviorInterface,
    };

enum UserAction {
  NEXT = 'next',
}

export class AiIntroScreen extends AiIntroScreenElementBase {
  static get is() {
    return 'ai-intro-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  override ready(): void {
    super.ready();
    this.initializeLoginScreen('AiIntroScreen');
  }

  override get defaultControl(): HTMLElement {
    const dialog =  this.shadowRoot?.querySelector('#aiIntroDialog');
    assertInstanceof(dialog, OobeAdaptiveDialog);
    return dialog;
  }

  private onNextClicked(): void {
    this.userActed(UserAction.NEXT);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [AiIntroScreen.is]: AiIntroScreen;
  }
}

customElements.define(AiIntroScreen.is, AiIntroScreen);
