// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './app_details_item.js';
import 'chrome://resources/cr_components/app_management/more_permissions_item.js';
import './pin_to_shelf_item.js';
import './app_management_cros_shared_style.css.js';

import {App, ExtensionAppPermissionMessage} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {getSelectedApp} from 'chrome://resources/cr_components/app_management/util.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AppManagementBrowserProxy} from './browser_proxy.js';
import {getTemplate} from './chrome_app_detail_view.html.js';
import {AppManagementStoreMixin} from './store_mixin.js';

const AppManagementChromeAppDetailViewElementBase =
    AppManagementStoreMixin(PolymerElement);

class AppManagementChromeAppDetailViewElement extends
    AppManagementChromeAppDetailViewElementBase {
  static get is() {
    return 'app-management-chrome-app-detail-view';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      app_: {
        type: Object,
        observer: 'onAppChanged_',
      },

      messages_: Object,
    };
  }

  private app_: App;
  private messages_: ExtensionAppPermissionMessage[];

  override connectedCallback(): void {
    super.connectedCallback();

    this.watch('app_', state => getSelectedApp(state));
    this.updateFromStore();
  }

  private async onAppChanged_() {
    try {
      const {messages: messages} =
          await AppManagementBrowserProxy.getInstance()
              .handler.getExtensionAppPermissionMessages(this.app_.id);
      this.messages_ = messages;
    } catch (err) {
      console.warn(err);
    }
  }

  private getPermissionMessages_(messages: ExtensionAppPermissionMessage[]):
      string[] {
    return messages.map(m => m.message);
  }

  private getPermissionSubmessagesByMessage_(
      index: number, messages: ExtensionAppPermissionMessage[]): string[]|null {
    // Dom-repeat still tries to access messages[0] when app has no
    // permission therefore we add an extra check.
    if (!messages[index]) {
      return null;
    }
    return messages[index].submessages;
  }

  private hasPermissions_(messages: ExtensionAppPermissionMessage[]): boolean {
    return messages.length > 0;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'app-management-chrome-app-detail-view':
        AppManagementChromeAppDetailViewElement;
  }
}

customElements.define(
    AppManagementChromeAppDetailViewElement.is,
    AppManagementChromeAppDetailViewElement);
