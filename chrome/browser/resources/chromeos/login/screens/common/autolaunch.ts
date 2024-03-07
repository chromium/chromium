// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Oobe reset screen implementation.
 */

import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/oobe_icons.html.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';
import '../../components/buttons/oobe_text_button.js';

import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.js';
import {OobeDialogHostBehavior, OobeDialogHostBehaviorInterface} from '../../components/behaviors/oobe_dialog_host_behavior.js';
import {OobeI18nMixin, OobeI18nMixinInterface} from '../../components/mixins/oobe_i18n_mixin.js';

import {getTemplate} from './autolaunch.html.js';

interface AppData {
  appName: string;
  appIconUrl: string;
}

const AutolaunchBase = mixinBehaviors(
    [LoginScreenBehavior, OobeDialogHostBehavior],
    OobeI18nMixin(PolymerElement)) as { new (): PolymerElement
      & OobeI18nMixinInterface
      & LoginScreenBehaviorInterface
      & OobeDialogHostBehaviorInterface,
    };

export class Autolaunch extends AutolaunchBase {
  static get is() {
    return 'autolaunch-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      appName: {
        type: String,
        value: '',
      },
      appIconUrl: {
        type: String,
        value: '',
      },
    };
  }

  private appName: string;
  private appIconUrl: string;

  constructor() {
    super();
  }

  override get EXTERNAL_API(): string[] {
    return [
      'updateApp',
    ];
  }

  override ready() {
    super.ready();
    this.initializeLoginScreen('AutolaunchScreen');
  }

  private onConfirm(): void {
    this.userActed('confirm');
  }

  private onCancel(): void {
    this.userActed('cancel');
  }

  /**
   * Event handler invoked when the page is shown and ready.
   */
  override onBeforeShow(): void {
    chrome.send('autolaunchVisible');
  }

  /**
   * Cancels the reset and drops the user back to the login screen.
   */
  private cancel(): void {
    this.userActed('cancel');
  }

  /**
   * Sets app to be displayed in the auto-launch warning.
   * @param app An dictionary with app info.
   */
  updateApp(app: AppData): void {
    this.appName = app.appName;
    if (app.appIconUrl && app.appIconUrl.length) {
      this.appIconUrl = app.appIconUrl;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [Autolaunch.is]: Autolaunch;
  }
}

customElements.define(Autolaunch.is, Autolaunch);
