// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/ash/common/cr_elements/cros_color_overrides.css.js';
import '../../components/buttons/oobe_next_button.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';

import {assertInstanceof} from '//resources/js/assert.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {OobeAdaptiveDialog} from '../../components/dialogs/oobe_adaptive_dialog.js';
import {LoginScreenMixin} from '../../components/mixins/login_screen_mixin.js';
import {OobeDialogHostMixin} from '../../components/mixins/oobe_dialog_host_mixin.js';
import {OobeI18nMixin} from '../../components/mixins/oobe_i18n_mixin.js';

import {getTemplate} from './split_modifier_keyboard_info.html.js';

export const SplitModifierKeyboardInfoScreenElementBase =
    OobeDialogHostMixin(LoginScreenMixin(OobeI18nMixin(PolymerElement)));

enum UserAction {
  NEXT = 'next',
}

export class SplitModifierKeyboardInfoScreen extends
    SplitModifierKeyboardInfoScreenElementBase {
  static get is() {
    return 'split-modifier-keyboard-info-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  override ready(): void {
    super.ready();
    this.initializeLoginScreen('SplitModifierKeyboardInfoScreen');
  }

  override get defaultControl(): HTMLElement {
    const dialog =
        this.shadowRoot?.querySelector('#splitModifierKeyboardInfoDialog');
    assertInstanceof(dialog, OobeAdaptiveDialog);
    return dialog;
  }

  private onNextClicked(): void {
    this.userActed(UserAction.NEXT);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SplitModifierKeyboardInfoScreen.is]: SplitModifierKeyboardInfoScreen;
  }
}

customElements.define(
    SplitModifierKeyboardInfoScreen.is, SplitModifierKeyboardInfoScreen);
