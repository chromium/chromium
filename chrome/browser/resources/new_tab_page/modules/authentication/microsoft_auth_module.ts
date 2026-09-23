// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import '../module_header.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {I18nMixinLit, loadTimeData} from '../../i18n_setup.js';
import {recordEnumeration} from '../../metrics_utils.js';
import {AuthType} from '../../ntp_microsoft_auth_shared_ui.mojom-webui.js';
import {ParentTrustedDocumentProxy} from '../microsoft_auth_frame_connector.js';
import {ModuleDescriptor} from '../module_descriptor.js';
import type {MenuItem, ModuleHeaderElement} from '../module_header.js';

import {getCss} from './microsoft_auth_module.css.js';
import {getHtml} from './microsoft_auth_module.html.js';


export interface MicrosoftAuthModuleElement {
  $: {
    moduleHeader: ModuleHeaderElement,
    signInButton: HTMLButtonElement,
  };
}

const MicrosoftAuthModuleElementBase = I18nMixinLit(CrLitElement);

/**
 * The Microsoft Authentication module, which enables users to sign in with
 * their Microsoft accounts and authenticate data retrieval from various
 * Microsoft services, such as Sharepoint and Outlook.
 */
export class MicrosoftAuthModuleElement extends MicrosoftAuthModuleElementBase {
  static get is() {
    return 'ntp-microsoft-auth-module';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  protected getMenuItems_(): MenuItem[] {
    return [
      {
        action: 'disable',
        icon: loadTimeData.getBoolean('webuiRoundedIconsEnabled') ?
            'modules:block' :
            'modules:block-old',
        text: this.i18n('modulesMicrosoftAuthDisable'),
      },
    ];
  }

  protected onDisableButtonClick_() {
    const disableEvent = new CustomEvent('disable-module', {
      bubbles: true,
      composed: true,
      detail: {
        message: loadTimeData.getStringF(
            'disableModuleToastMessage',
            loadTimeData.getString('modulesMicrosoftAuthName')),
      },
    });
    this.dispatchEvent(disableEvent);
  }

  // Cause Login flow to begin within auth iframe.
  protected onSignInClick_() {
    const proxyInstance = ParentTrustedDocumentProxy.getInstance();
    if (proxyInstance) {
      proxyInstance.getChildDocument().acquireTokenPopup();
      recordEnumeration(
          `NewTabPage.MicrosoftAuth.AuthStarted`, AuthType.kPopup,
          AuthType.MAX_VALUE + 1);
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ntp-microsoft-auth-module': MicrosoftAuthModuleElement;
  }
}

customElements.define(
    MicrosoftAuthModuleElement.is, MicrosoftAuthModuleElement);

function createMicrosoftAuthElement():
    Promise<MicrosoftAuthModuleElement|null> {
  return Promise.resolve(new MicrosoftAuthModuleElement());
}

export const microsoftAuthModuleDescriptor: ModuleDescriptor =
    new ModuleDescriptor(
        /*id*/ 'microsoft_authentication', createMicrosoftAuthElement);
