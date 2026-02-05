// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './event_list/event_list.js';
import './updater_state/updater_state.js';
import './app_list/app_list.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import {BrowserProxyImpl} from './browser_proxy.js';
import type {EnterpriseCompanionState, GetEnterpriseCompanionStateResponse, GetUpdaterStatesResponse, UpdaterState} from './updater_ui.mojom-webui.js';

export class UpdaterAppElement extends CrLitElement {
  static get is() {
    return 'updater-app';
  }

  static override get styles() {
    return getCss();
  }

  static override get properties() {
    return {
      messages: {type: Array},
      userUpdaterState: {type: Object},
      systemUpdaterState: {type: Object},
      enterpriseCompanionState: {type: Object},
      updaterStateError: {type: Boolean},
    };
  }

  protected accessor messages: Array<Record<string, unknown>> = [];
  protected accessor userUpdaterState: UpdaterState|null = null;
  protected accessor systemUpdaterState: UpdaterState|null = null;
  protected accessor enterpriseCompanionState: EnterpriseCompanionState|null =
      null;
  protected accessor updaterStateError = false;


  override connectedCallback() {
    super.connectedCallback();
    this.getAllUpdaterEvents().then(messages => this.messages = messages);
    this.getUpdaterStates()
        .then(response => {
          this.userUpdaterState = response.user;
          this.systemUpdaterState = response.system;
        })
        .catch(() => {
          this.updaterStateError = true;
        });
    this.getEnterpriseCompanionState()
        .then(response => {
          this.enterpriseCompanionState = response.state;
        })
        .catch(() => {
          this.updaterStateError = true;
        });
  }

  override render() {
    return getHtml.bind(this)();
  }

  private async getAllUpdaterEvents(): Promise<Array<Record<string, unknown>>> {
    const response =
        await BrowserProxyImpl.getInstance().handler.getAllUpdaterEvents();

    return response.events.map(message => JSON.parse(message))
        .filter(message => typeof message === 'object');
  }

  private async getUpdaterStates(): Promise<GetUpdaterStatesResponse> {
    return await BrowserProxyImpl.getInstance().handler.getUpdaterStates();
  }

  private async getEnterpriseCompanionState():
      Promise<GetEnterpriseCompanionStateResponse> {
    return await BrowserProxyImpl.getInstance()
        .handler.getEnterpriseCompanionState();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'updater-app': UpdaterAppElement;
  }
}

customElements.define(UpdaterAppElement.is, UpdaterAppElement);
