// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './app_list/app_list.js';
import './enterprise_policy_table/enterprise_policy_table.js';
import './event_list/event_list.js';
import './updater_state/updater_state.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import type {AppStateDisplay} from './app_list/app_list.js';
import {BrowserProxyImpl} from './browser_proxy.js';
import {parsePolicySet} from './event_history.js';
import type {PolicySet} from './event_history.js';
import {getKnownAppNamesById} from './known_apps.js';
import type {EnterpriseCompanionState, GetAppStatesResponse, GetEnterpriseCompanionStateResponse, GetUpdaterStatesResponse, UpdaterState} from './updater_ui.mojom-webui.js';

export class UpdaterAppElement extends CrLitElement {
  static get is() {
    return 'updater-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      messages: {type: Array},
      userUpdaterState: {type: Object},
      systemUpdaterState: {type: Object},
      enterpriseCompanionState: {type: Object},
      updaterStateError: {type: Boolean},
      apps: {type: Array},
      appStateError: {type: Boolean},
    };
  }

  accessor messages: Array<Record<string, unknown>> = [];
  accessor userUpdaterState: UpdaterState|null = null;
  accessor systemUpdaterState: UpdaterState|null = null;
  accessor enterpriseCompanionState: EnterpriseCompanionState|null = null;
  accessor updaterStateError = false;
  accessor apps: AppStateDisplay[] = [];
  accessor appStateError = false;

  protected policies: PolicySet|undefined = undefined;


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
    this.getAppStates()
        .then(apps => this.apps = apps)
        .catch(() => this.appStateError = true);
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);
    if (changedProperties.has('systemUpdaterState')) {
      this.policies = this.computePolicies();
    }
  }

  private computePolicies(): PolicySet|undefined {
    if (this.systemUpdaterState === null) {
      return undefined;
    }
    const policies = JSON.parse(this.systemUpdaterState.policies);
    try {
      return parsePolicySet({policies}, 'policies');
    } catch (e) {
      console.warn(`Failed to parse policy set: ${e}. Message: ${policies}`);
      return undefined;
    }
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

  private async getAppStates(): Promise<AppStateDisplay[]> {
    const response: GetAppStatesResponse =
        await BrowserProxyImpl.getInstance().handler.getAppStates();
    const knownApps = getKnownAppNamesById();

    const systemApps: AppStateDisplay[] = response.systemApps.map(
        app => ({
          ...app,
          scope: 'SYSTEM',
          displayName: knownApps.get(app.appId.toLowerCase()) || app.appId,
        }));
    const userApps: AppStateDisplay[] = response.userApps.map(
        app => ({
          ...app,
          scope: 'USER',
          displayName: knownApps.get(app.appId.toLowerCase()) || app.appId,
        }));
    return [...systemApps, ...userApps];
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'updater-app': UpdaterAppElement;
  }
}

customElements.define(UpdaterAppElement.is, UpdaterAppElement);
