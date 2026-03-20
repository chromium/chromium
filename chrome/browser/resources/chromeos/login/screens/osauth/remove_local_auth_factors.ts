// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/ash/common/cr_elements/cros_color_overrides.css.js';
import '../../components/buttons/oobe_back_button.js';
import '../../components/buttons/oobe_next_button.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';

import type {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenMixin} from '../../components/mixins/login_screen_mixin.js';
import {MultiStepMixin} from '../../components/mixins/multi_step_mixin.js';
import {OobeDialogHostMixin} from '../../components/mixins/oobe_dialog_host_mixin.js';
import {OobeI18nMixin} from '../../components/mixins/oobe_i18n_mixin.js';

import {getTemplate} from './remove_local_auth_factors.html.js';

const RemoveLocalAuthFactorsScreenElementBase =
    LoginScreenMixin(MultiStepMixin(OobeI18nMixin(PolymerElement)));

enum UserAction {
  DONE = 'done',
}

enum RemoveLocalAuthFactorsScreenState {
  SUCCESS = 'success',
  PROGRESS = 'progress',
}

class RemoveLocalAuthFactorsScreen extends
    RemoveLocalAuthFactorsScreenElementBase {
  static get is() {
    return 'remove-local-auth-factors-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {};
  }


  override get EXTERNAL_API(): string[] {
    return ['showRemoveLocalAuthFactorsSuccessStep'];
  }

  override defaultUIStep(): string {
    return RemoveLocalAuthFactorsScreenState.PROGRESS;
  }

  override get UI_STEPS() {
    return RemoveLocalAuthFactorsScreenState;
  }

  override ready(): void {
    super.ready();
    this.initializeLoginScreen('RemoveLocalAuthFactorsScreen');
  }

  /**
   * Next button click handler.
   */
  private onDoneClicked(): void {
    this.userActed(UserAction.DONE);
  }

  private showRemoveLocalAuthFactorsSuccessStep(): void {
    this.setUIStep(RemoveLocalAuthFactorsScreenState.SUCCESS);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [RemoveLocalAuthFactorsScreen.is]: RemoveLocalAuthFactorsScreen;
  }
}

customElements.define(
    RemoveLocalAuthFactorsScreen.is, RemoveLocalAuthFactorsScreen);
