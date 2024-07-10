// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying ARC ADB sideloading screen.
 */

import '//resources/js/action_link.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/buttons/oobe_text_button.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';
import '../../components/oobe_icons.html.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenMixin} from '../../components/mixins/login_screen_mixin.js';
import {MultiStepMixin} from '../../components/mixins/multi_step_mixin.js';
import {OobeDialogHostMixin} from '../../components/mixins/oobe_dialog_host_mixin.js';
import {OobeI18nMixin} from '../../components/mixins/oobe_i18n_mixin.js';

import {getTemplate} from './adb_sideloading.html.js';

/**
 * UI mode for the dialog.
 */
enum AdbSideloadingState {
  ERROR = 'error',
  SETUP = 'setup',
}

/**
 * The constants need to be synced with EnableAdbSideloadingScreenView::UIState
 */
enum AdbsideloadingScreenState {
  ERROR = 1,
  SETUP = 2,
}

const AdbSideloadingBase = OobeDialogHostMixin(
    LoginScreenMixin(MultiStepMixin(OobeI18nMixin(PolymerElement))));

export class AdbSideloading extends AdbSideloadingBase {
  static get is() {
    return 'adb-sideloading-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  constructor() {
    super();
  }

  override get EXTERNAL_API(): string[] {
    return ['setScreenState'];
  }

  override get UI_STEPS() {
    return AdbSideloadingState;
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  override defaultUIStep() {
    return AdbSideloadingState.SETUP;
  }

  override ready(): void {
    super.ready();
    this.initializeLoginScreen('EnableAdbSideloadingScreen');
  }

  /*
   * Executed on language change.
   */
  override updateLocalizedContent(): void {
    this.i18nUpdateLocale();
  }

  override onBeforeShow(): void {
    super.onBeforeShow();
    this.setScreenState(AdbsideloadingScreenState.SETUP);
  }

  /**
   * Sets UI state for the dialog to show corresponding content.
   */
  setScreenState(state: AdbsideloadingScreenState): void {
    if (state === AdbsideloadingScreenState.ERROR) {
      this.setUIStep(AdbSideloadingState.ERROR);
    } else if (state === AdbsideloadingScreenState.SETUP) {
      this.setUIStep(AdbSideloadingState.SETUP);
    }
  }

  /**
   * On-tap event handler for enable button.
   */
  private onEnableClick(): void {
    this.userActed('enable-pressed');
  }

  /**
   * On-tap event handler for cancel button.
   */
  private onCancelClick(): void {
    this.userActed('cancel-pressed');
  }

  /**
   * On-tap event handler for learn more link.
   */
  private onLearnMoreClick(): void {
    this.userActed('learn-more-link');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [AdbSideloading.is]: AdbSideloading;
  }
}

customElements.define(AdbSideloading.is, AdbSideloading);
