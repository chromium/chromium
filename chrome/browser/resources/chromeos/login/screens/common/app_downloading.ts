// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design App Downloading
 * screen.
 */

import '//resources/ash/common/cr_elements/cr_checkbox/cr_checkbox.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/oobe_cr_lottie.js';
import '../../components/buttons/oobe_text_button.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';

import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.js';
import {OobeDialogHostBehavior, OobeDialogHostBehaviorInterface} from '../../components/behaviors/oobe_dialog_host_behavior.js';
import {OobeI18nBehavior, OobeI18nBehaviorInterface} from '../../components/behaviors/oobe_i18n_behavior.js';
import {OobeUiState} from '../../components/display_manager_types.js';
import {OobeCrLottie} from '../../components/oobe_cr_lottie.js';

import {getTemplate} from './app_downloading.html.js';

const AppDownloadingBase = mixinBehaviors(
    [OobeI18nBehavior, OobeDialogHostBehavior, LoginScreenBehavior],
    PolymerElement) as {
      new (): PolymerElement & OobeI18nBehaviorInterface
        & OobeDialogHostBehaviorInterface & LoginScreenBehaviorInterface,
    };

export class AppDownloading extends AppDownloadingBase {
  static get is() {
    return 'app-downloading-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {};
  }

  override ready(): void {
    super.ready();
    this.initializeLoginScreen('AppDownloadingScreen');
  }

  /** Initial UI State for screen */
  // eslint-disable-next-line @typescript-eslint/naming-convention
  override getOobeUIInitialState(): OobeUiState {
    return OobeUiState.ONBOARDING;
  }

  /**
   * Returns the control which should receive initial focus.
   */
  override get defaultControl(): HTMLElement {
    return this.shadowRoot!.querySelector('#app-downloading-dialog')!;
  }

  /** Called when dialog is shown */
  override onBeforeShow(): void {
    const downloadingApps = this.getDownloadingAppsLottiePlayer();
    if (downloadingApps instanceof OobeCrLottie) {
      downloadingApps.playing = true;
    }
  }

  /** Called when dialog is hidden */
  onBeforeHide(): void {
    const downloadingApps = this.getDownloadingAppsLottiePlayer();
    if (downloadingApps instanceof OobeCrLottie) {
      downloadingApps.playing = false;
    }
  }

  onContinue(): void {
    this.userActed('appDownloadingContinueSetup');
  }

  private getDownloadingAppsLottiePlayer(): OobeCrLottie | null | undefined {
    return this.shadowRoot?.querySelector('#downloadingApps');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [AppDownloading.is]: AppDownloading;
  }
}

customElements.define(AppDownloading.is, AppDownloading);
