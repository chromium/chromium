// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/policy/cr_tooltip_icon.js';
import '//resources/cr_elements/icons_lit.html.js';

import type {App} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {BrowserProxy} from 'chrome://resources/cr_components/app_management/browser_proxy.js';
import {AppManagementUserAction, InstallReason} from 'chrome://resources/cr_components/app_management/constants.js';
import {recordAppManagementUserAction} from 'chrome://resources/cr_components/app_management/util.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './uninstall_button.css.js';
import {getHtml} from './uninstall_button.html.js';
import {createDummyApp} from './web_app_settings_utils.js';

export class UninstallButtonElement extends CrLitElement {
  static get is() {
    return 'app-management-uninstall-button';
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
      uninstallLabel: {type: String},
      policyLabel: {type: String},
    };
  }

  app: App = createDummyApp();
  uninstallLabel: string = '';
  policyLabel: string = '';

  /**
   * Returns true if the button should be disabled due to app install type.
   *
   * If the compiler complains about the "lack of ending return statement",
   * you maybe just added a new InstallReason and need to add a new case.
   */
  protected getDisableState_(): boolean {
    switch (this.app.installReason) {
      case InstallReason.kSystem:
      case InstallReason.kPolicy:
      case InstallReason.kKiosk:
        return true;
      case InstallReason.kUnknown:
      case InstallReason.kOem:
      case InstallReason.kDefault:
      case InstallReason.kSubApp:
      case InstallReason.kSync:
      case InstallReason.kUser:
      case InstallReason.kSubApp:
      case InstallReason.kCommandLine:
        return false;
    }
  }

  /**
   * Returns true if the app was installed by a policy.
   */
  protected showPolicyIndicator_(): boolean {
    return this.app.installReason === InstallReason.kPolicy;
  }

  /**
   * Returns true if the uninstall button should be shown.
   */
  protected showUninstallButton_(): boolean {
    return this.app.installReason !== InstallReason.kSystem;
  }

  protected onClick_() {
    BrowserProxy.getInstance().handler.uninstall(this.app.id);
    recordAppManagementUserAction(
        this.app.type, AppManagementUserAction.UNINSTALL_DIALOG_LAUNCHED);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'app-management-uninstall-button': UninstallButtonElement;
  }
}

customElements.define(UninstallButtonElement.is, UninstallButtonElement);
