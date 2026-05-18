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

interface RemoveLocalAuthFactorsScreenData {
  email: string;
}

enum UserAction {
  DONE = 'done',
}

enum RemoveLocalAuthFactorsScreenState {
  SUCCESS = 'success',
  PROGRESS = 'progress',
  DONE = 'done',
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
    return {
      // User email address.
      email: {type: String, value: ''},
    };
  }

  private email: string = '';

  override get EXTERNAL_API(): string[] {
    return ['showRemoveLocalAuthFactorsSuccessStep'];
  }


  /**
   * Event handler that is invoked just before the frame is shown.
   * data contains email string which is used for localization.
   */
  override onBeforeShow(data: RemoveLocalAuthFactorsScreenData): void {
    super.onBeforeShow(data);
    this.email = data.email;
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

  private isSuccessStep(uiStep: RemoveLocalAuthFactorsScreenState) {
    return uiStep === RemoveLocalAuthFactorsScreenState.SUCCESS;
  }

  /**
   * Done button click handler, Done button is only shown when the screen is in
   * SUCCESS state.
   */
  private onDoneClicked(): void {
    if (this.uiStep !== RemoveLocalAuthFactorsScreenState.SUCCESS) {
      return;
    }
    this.setUIStep(RemoveLocalAuthFactorsScreenState.DONE);
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
