// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../../module_header.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {I18nMixinLit, loadTimeData} from '../../../i18n_setup.js';
import type {MicrosoftAuthPageHandlerRemote} from '../../../microsoft_auth.mojom-webui.js';
import {ModuleDescriptor} from '../../module_descriptor.js';
import type {MenuItem, ModuleHeaderElement} from '../module_header.js';

import {getCss} from './microsoft_auth_module.css.js';
import {getHtml} from './microsoft_auth_module.html.js';
import {MicrosoftAuthProxyImpl} from './microsoft_auth_module_proxy.js';


export interface MicrosoftAuthModuleElement {
  $: {
    moduleHeaderElementV2: ModuleHeaderElement,
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
    return 'ntp-microsoft-authentication-module';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  private handler_: MicrosoftAuthPageHandlerRemote;

  constructor() {
    super();
    this.handler_ = MicrosoftAuthProxyImpl.getInstance().handler;
  }

  protected getMenuItemGroups_(): MenuItem[][] {
    return [
      [
        {
          action: 'dismiss',
          icon: 'modules:visibility_off',
          text: this.i18n('modulesMicrosoftAuthDismiss'),
        },
        {
          action: 'disable',
          icon: 'modules:block',
          text: this.i18n('modulesMicrosoftAuthDisable'),
        },
      ],
      [
        {
          action: 'customize-module',
          icon: 'modules:tune',
          text: this.i18n('modulesCustomizeButtonText'),
        },
      ],
    ];
  }

  protected onDisableButtonClick_() {
    const disableEvent = new CustomEvent('disable-module', {
      composed: true,
      detail: {
        message: loadTimeData.getStringF(
            'disableModuleToastMessage',
            loadTimeData.getString('modulesMicrosoftAuthName')),
      },
    });
    this.dispatchEvent(disableEvent);
  }

  protected onDismissButtonClick_() {
    this.handler_.dismissModule();
    this.dispatchEvent(new CustomEvent('dismiss-module-instance', {
      bubbles: true,
      composed: true,
      detail: {
        message: loadTimeData.getStringF(
            'dismissModuleToastMessage',
            loadTimeData.getString('modulesMicrosoftAuthName')),
        restoreCallback: () => this.handler_.restoreModule(),
      },
    }));
  }

  protected onSignInClick_() {
    // TODO(crbug.com/377379069): Handle button click.
  }
}

customElements.define(
    MicrosoftAuthModuleElement.is, MicrosoftAuthModuleElement);

export const microsoftAuthModuleDescriptor: ModuleDescriptor =
    new ModuleDescriptor(
        /*id*/ 'microsoft_authentication',
        async(): Promise<MicrosoftAuthModuleElement> =>
            new MicrosoftAuthModuleElement());
