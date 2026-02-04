// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/icons.html.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {BrowserProxyImpl} from '../browser_proxy.js';
import {ShowDirectoryTarget} from '../updater_ui.mojom-webui.js';

import {getCss} from './enterprise_companion_state_card.css.js';
import {getHtml} from './enterprise_companion_state_card.html.js';

export class EnterpriseCompanionStateCardElement extends CrLitElement {
  static get is() {
    return 'enterprise-companion-state-card';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      version: {type: String},
      installPath: {type: String},
    };
  }

  accessor version: string|undefined = undefined;
  accessor installPath: string|undefined = undefined;

  protected onInstallPathClick() {
    BrowserProxyImpl.getInstance().handler.showDirectory(
        ShowDirectoryTarget.kEnterpriseCompanionApp);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'enterprise-companion-state-card': EnterpriseCompanionStateCardElement;
  }
}

customElements.define(
    EnterpriseCompanionStateCardElement.is,
    EnterpriseCompanionStateCardElement);
