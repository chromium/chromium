// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for install attributes error screen.
 */

import '//resources/cr_elements/cr_shared_vars.css.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/oobe_icons.html.js';
import '../../components/buttons/oobe_text_button.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';

import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.js';
import {OobeI18nBehavior, OobeI18nBehaviorInterface} from '../../components/behaviors/oobe_i18n_behavior.js';

import {getTemplate} from './install_attributes_error.html.js';

const InstallAttributesErrorMessageElementBase =
    mixinBehaviors([OobeI18nBehavior, LoginScreenBehavior], PolymerElement) as {
      new (): PolymerElement & OobeI18nBehaviorInterface &
          LoginScreenBehaviorInterface,
    };

/**
 * Data that is passed to the screen during onBeforeShow.
 */
interface InstallAttributesErrorScreenData {
  restartRequired: boolean;
}

export class InstallAttributesErrorMessage extends
    InstallAttributesErrorMessageElementBase {
  static get is() {
    return 'install-attributes-error-message-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      /** Whether the restart is required for powerwash to be available. */
      isRestartRequired: {
        type: Boolean,
        value: false,
      },
    };
  }

  private isRestartRequired: boolean;

  constructor() {
    super();
    this.isRestartRequired = false;
  }

  override ready(): void {
    super.ready();
    this.initializeLoginScreen('InstallAttributesErrorMessageScreen');
  }

  private onPowerwash(): void {
    this.userActed('powerwash-pressed');
  }

  private onRestart(): void {
    this.userActed('reboot-system');
  }

  onBeforeShow(data: InstallAttributesErrorScreenData): void {
    this.isRestartRequired = data['restartRequired'];
  }

  override get defaultControl(): HTMLElement|null {
    return this.shadowRoot!.querySelector('#errorDialog');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [InstallAttributesErrorMessage.is]: InstallAttributesErrorMessage;
  }
}

customElements.define(
    InstallAttributesErrorMessage.is, InstallAttributesErrorMessage);
