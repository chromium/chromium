// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @fileoverview Polymer element for displaying material design Enable Kiosk
 * screen.
 */

import '//resources/cr_elements/icons.html.js';
import '../../components/buttons/oobe_text_button.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';

import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.js';
import {OobeDialogHostBehavior, OobeDialogHostBehaviorInterface} from '../../components/behaviors/oobe_dialog_host_behavior.js';
import {OobeI18nBehavior, OobeI18nBehaviorInterface} from '../../components/behaviors/oobe_i18n_behavior.js';

import {getTemplate} from './enable_kiosk.html.js';

/**
 * UI mode for the dialog.
 */
enum EnableKioskMode {
  CONFIRM = 'confirm',
  SUCCESS = 'success',
  ERROR = 'error',
}

export const EnableKioskBase =
    mixinBehaviors(
        [OobeI18nBehavior, LoginScreenBehavior, OobeDialogHostBehavior],
        PolymerElement) as {
      new (): PolymerElement & OobeI18nBehaviorInterface &
          LoginScreenBehaviorInterface & OobeDialogHostBehaviorInterface,
    };

export class EnableKiosk extends EnableKioskBase {
  static get is() {
    return 'enable-kiosk-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      /**
       * Current dialog state
       */
      state_: {
        value: EnableKioskMode.CONFIRM,
      },
    };
  }

  private state_: EnableKioskMode;

  constructor() {
    super();
  }

  override get EXTERNAL_API(): string[] {
    return ['onCompleted'];
  }

  override ready(): void {
    super.ready();
    this.initializeLoginScreen('KioskEnableScreen');
  }

  /** Called after resources are updated. */
  override updateLocalizedContent(): void {
    this.i18nUpdateLocale();
  }

  /** Called when dialog is shown */
  override onBeforeShow(): void {
    this.state_ = EnableKioskMode.CONFIRM;
  }

  /**
   * "Enable" button handler
   */
  private onEnableButton_(): void {
    this.userActed('enable');
  }

  /**
   * "Cancel" / "Ok" button handler
   */
  private closeDialog_(): void {
    this.userActed('close');
  }

  onCompleted(success: boolean): void {
    this.state_ = success ? EnableKioskMode.SUCCESS : EnableKioskMode.ERROR;
  }

  /**
   * Simple equality comparison function.
   */
  private eq_(one: EnableKioskMode, another: string): boolean {
    return one === another;
  }

  private primaryButtonTextKey_(state: EnableKioskMode): string {
    if (state === EnableKioskMode.CONFIRM) {
      return 'kioskOKButton';
    }
    return 'kioskCancelButton';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [EnableKiosk.is]: EnableKiosk;
  }
}

customElements.define(EnableKiosk.is, EnableKiosk);
