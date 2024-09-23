// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for install attributes error screen.
 */

import '//resources/ash/common/cr_elements/cr_shared_vars.css.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/oobe_icons.html.js';
import '../../components/buttons/oobe_text_button.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';

import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenMixin} from '../../components/mixins/login_screen_mixin.js';
import {OobeI18nMixin} from '../../components/mixins/oobe_i18n_mixin.js';

import {getTemplate} from './install_attributes_error.html.js';

const InstallAttributesErrorMessageElementBase =
    LoginScreenMixin(OobeI18nMixin(PolymerElement));

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

  override onBeforeShow(data: InstallAttributesErrorScreenData): void {
    super.onBeforeShow(data);
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
