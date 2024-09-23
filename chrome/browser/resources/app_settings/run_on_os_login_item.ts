// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './app_management_shared_style.css.js';
import './toggle_row.js';

import {assert, assertNotReached} from '//resources/js/assert.js';
import type {App} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {BrowserProxy} from 'chrome://resources/cr_components/app_management/browser_proxy.js';
import {AppManagementUserAction, RunOnOsLoginMode} from 'chrome://resources/cr_components/app_management/constants.js';
import {recordAppManagementUserAction} from 'chrome://resources/cr_components/app_management/util.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app_management_shared_style.css.js';
import {getHtml} from './run_on_os_login_item.html.js';
import type {ToggleRowElement} from './toggle_row.js';
import {createDummyApp} from './web_app_settings_utils.js';

function convertModeToBoolean(runOnOsLoginMode: RunOnOsLoginMode): boolean {
  switch (runOnOsLoginMode) {
    case RunOnOsLoginMode.kNotRun:
      return false;
    case RunOnOsLoginMode.kWindowed:
      return true;
    default:
      assertNotReached();
  }
}

function getRunOnOsLoginModeBoolean(runOnOsLoginMode: RunOnOsLoginMode):
    boolean {
  assert(
      runOnOsLoginMode !== RunOnOsLoginMode.kUnknown,
      'Run on OS Login Mode is not set');
  return convertModeToBoolean(runOnOsLoginMode);
}

export class RunOnOsLoginItemElement extends CrLitElement {
  static get is() {
    return 'app-management-run-on-os-login-item';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      app: {type: Object},
      loginModeLabel: {type: String},
    };
  }

  loginModeLabel: string = '';
  app: App = createDummyApp();

  override firstUpdated() {
    this.addEventListener('click', this.onClick_);
    this.addEventListener('change', this.toggleOsLoginMode_);
  }

  protected isManaged_(): boolean {
    const loginData = this.app.runOnOsLogin;
    if (loginData) {
      return loginData.isManaged;
    }
    return false;
  }

  protected getValue_(): boolean {
    const loginMode = this.getRunOnOsLoginMode();
    assert(loginMode);

    if (loginMode) {
      return getRunOnOsLoginModeBoolean(loginMode);
    }
    return false;
  }

  private onClick_() {
    this.shadowRoot!.querySelector<ToggleRowElement>('#toggle-row')!.click();
  }

  private toggleOsLoginMode_() {
    assert(this.app);
    const currentRunOnOsLoginData = this.app.runOnOsLogin;
    if (currentRunOnOsLoginData) {
      const currentRunOnOsLoginMode = currentRunOnOsLoginData.loginMode;
      if (currentRunOnOsLoginMode === RunOnOsLoginMode.kUnknown) {
        assertNotReached();
      }
      const newRunOnOsLoginMode =
          (currentRunOnOsLoginMode === RunOnOsLoginMode.kNotRun) ?
          RunOnOsLoginMode.kWindowed :
          RunOnOsLoginMode.kNotRun;
      BrowserProxy.getInstance().handler.setRunOnOsLoginMode(
          this.app.id,
          newRunOnOsLoginMode,
      );
      const booleanRunOnOsLoginMode =
          getRunOnOsLoginModeBoolean(newRunOnOsLoginMode);
      const runOnOsLoginModeChangeAction = booleanRunOnOsLoginMode ?
          AppManagementUserAction.RUN_ON_OS_LOGIN_MODE_TURNED_ON :
          AppManagementUserAction.RUN_ON_OS_LOGIN_MODE_TURNED_OFF;
      recordAppManagementUserAction(
          this.app.type, runOnOsLoginModeChangeAction);
    }
  }

  private getRunOnOsLoginMode(): RunOnOsLoginMode|null {
    if (this.app.runOnOsLogin) {
      return this.app.runOnOsLogin.loginMode;
    }
    return null;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'app-management-run-on-os-login-item': RunOnOsLoginItemElement;
  }
}

customElements.define(RunOnOsLoginItemElement.is, RunOnOsLoginItemElement);
