// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Oobe Assistant OptIn Flow screen implementation.
 */

import '../../assistant_optin/assistant_optin_flow.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {OobeUiState} from '../../components/display_manager_types.js';
// TODO(b/320439437) Migrate AssistantOptInFlow to ts
// import {AssistantOptInFlow} from
// '../../assistant_optin/assistant_optin_flow.js'
import {LoginScreenMixin} from '../../components/mixins/login_screen_mixin.js';
import {OobeDialogHostMixin} from '../../components/mixins/oobe_dialog_host_mixin.js';
import {OobeI18nMixin} from '../../components/mixins/oobe_i18n_mixin.js';

import {getTemplate} from './assistant_optin.html.js';

const AssistantOptinBase =
    OobeDialogHostMixin(LoginScreenMixin(OobeI18nMixin(PolymerElement)));

export class AssistantOptin extends AssistantOptinBase {
  static get is() {
    return 'assistant-optin-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  override get EXTERNAL_API(): string[] {
    return [
      'reloadContent',
      'addSettingZippy',
      'showNextScreen',
      'onVoiceMatchUpdate',
      'onValuePropUpdate',
    ];
  }

  override ready(): void {
    super.ready();
    this.initializeLoginScreen('AssistantOptInFlowScreen');
  }

  /**
   * Returns default event target element.
   */
  override get defaultControl(): HTMLElement|null {
    return this.shadowRoot!.querySelector('#card');
  }

  /** Initial UI State for screen */
  // eslint-disable-next-line @typescript-eslint/naming-convention
  override getOobeUIInitialState(): OobeUiState {
    return OobeUiState.ONBOARDING;
  }

  /**
   * Event handler that is invoked just before the frame is shown.
   */
  override onBeforeShow(): void {
    super.onBeforeShow();
    const card = this.shadowRoot?.querySelector('#card');
    if (card) {
      (card as any).onShow();
    }
  }

  /**
   * Reloads localized strings.
   * @param data New dictionary with i18n values.
   */
  reloadContent(data: Object): void {
    const card = this.shadowRoot?.querySelector('#card');
    if (card) {
      (card as any).reloadContent(data);
    }
  }

  /**
   * Add a setting zippy object in the corresponding screen.
   * @param type type of the setting zippy.
   * @param data String and url for the setting zippy.
   */
  addSettingZippy(type: string, data: Object): void {
    const card = this.shadowRoot?.querySelector('#card');
    if (card) {
      (card as any).addSettingZippy(type, data);
    }
  }

  /**
   * Show the next screen in the flow.
   */
  showNextScreen(): void {
    const card = this.shadowRoot?.querySelector('#card');
    if (card) {
      (card as any).showNextScreen();
    }
  }

  /**
   * Called when the Voice match state is updated.
   * @param state the voice match state.
   */
  onVoiceMatchUpdate(state: string): void {
    const card = this.shadowRoot?.querySelector('#card');
    if (card) {
      (card as any).onVoiceMatchUpdate(state);
    }
  }

  /**
   * Called to show the next settings when there are multiple unbundled
   * activity control settings in the Value prop screen.
   */
  onValuePropUpdate(): void {
    const card = this.shadowRoot?.querySelector('#card');
    if (card) {
      (card as any).onValuePropUpdate();
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [AssistantOptin.is]: AssistantOptin;
  }
}

customElements.define(AssistantOptin.is, AssistantOptin);
