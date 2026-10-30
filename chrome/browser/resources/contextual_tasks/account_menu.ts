// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_action_menu/cr_action_menu.js';

import {AnchorAlignment} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './account_menu.css.js';
import {getHtml} from './account_menu.html.js';
import type {ToolbarBrowserProxy} from './contextual_tasks_toolbar_browser_proxy.js';
import {ToolbarBrowserProxyImpl} from './contextual_tasks_toolbar_browser_proxy.js';
import {recordAction} from './utils.js';

export interface AccountMenuElement {
  $: {menu: CrActionMenuElement};
}

// Menu shown when the user clicks on the profile avatar in the side panel
// toolbar.
export class AccountMenuElement extends CrLitElement {
  static get is() {
    return 'contextual-tasks-account-menu';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      fullName: {type: String},
      email: {type: String},
    };
  }

  accessor fullName: string = '';
  accessor email: string = '';

  private toolbarBrowserProxy_: ToolbarBrowserProxy =
      ToolbarBrowserProxyImpl.getInstance();
  private isUnboundedMenuEnabled_: boolean =
      loadTimeData.valueExists('contextualTasksUnboundedMenuEnabled') &&
      loadTimeData.getBoolean('contextualTasksUnboundedMenuEnabled');

  async showAt(target: HTMLElement) {
    if (this.isUnboundedMenuEnabled_) {
      await this.$.menu.setUnbounded();
    }
    this.$.menu.showAt(target, {
      noOffset: true,
      anchorAlignmentY: AnchorAlignment.AFTER_END,
    });
  }

  close() {
    this.$.menu.close();
  }

  protected onManageAccountClick_() {
    this.close();
    recordAction(
        'ContextualTasks.WebUI.UserAction.ManageAccountFromAccountMenu');
    this.toolbarBrowserProxy_.handler.openManageAccountUi();
  }

  protected onSignOutClick_() {
    this.close();
    recordAction('ContextualTasks.WebUI.UserAction.SignOutFromAccountMenu');
    this.toolbarBrowserProxy_.handler.openLogOutFlow();
  }

  protected getAccountLabel_(): string {
    if (!this.fullName) {
      return this.email;
    }
    return loadTimeData.getStringF(
        'accountMenuNameAndEmail', this.fullName, this.email);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'contextual-tasks-account-menu': AccountMenuElement;
  }
}

customElements.define(AccountMenuElement.is, AccountMenuElement);
