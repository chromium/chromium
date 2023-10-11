// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar.js';
import 'chrome://resources/cr_components/app_management/app_content_item.js';
import 'chrome://resources/cr_components/app_management/app_management_shared_style.css.js';
import 'chrome://resources/cr_components/app_management/file_handling_item.js';
import 'chrome://resources/cr_components/app_management/more_permissions_item.js';
import 'chrome://resources/cr_components/app_management/run_on_os_login_item.js';
import 'chrome://resources/cr_components/app_management/permission_item.js';
import 'chrome://resources/cr_components/app_management/window_mode_item.js';
import 'chrome://resources/cr_components/app_management/icons.html.js';
import 'chrome://resources/cr_components/app_management/uninstall_button.js';
import 'chrome://resources/cr_components/localized_link/localized_link.js';
import 'chrome://resources/cr_components/app_management/supported_links_item.js';
import 'chrome://resources/cr_components/app_management/supported_links_overlapping_apps_dialog.js';
import 'chrome://resources/cr_components/app_management/supported_links_dialog.js';

import {App} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {BrowserProxy} from 'chrome://resources/cr_components/app_management/browser_proxy.js';
import {AppMap} from 'chrome://resources/cr_components/app_management/constants.js';
import {getAppIcon} from 'chrome://resources/cr_components/app_management/util.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';

const WebAppSettingsAppElementBase = I18nMixin(PolymerElement);

// TODO(crbug.com/1294060): Investigate end-to-end WebAppSettings tests
export class WebAppSettingsAppElement extends WebAppSettingsAppElementBase {
  static get is() {
    return 'web-app-settings-app';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      app_: Object,
      hidden: {
        type: Boolean,
        computed: 'appUnready_(app_)',
        reflectToAttribute: true,
      },
      iconUrl_: {type: String, computed: 'getAppIcon_(app_)'},
      showSearch_: {type: Boolean, value: false, readonly: true},
      apps_: Object,
    };
  }

  private app_: App|null;
  private apps_: AppMap;
  private iconUrl_: string;
  private showSearch_: boolean;

  override connectedCallback() {
    this.apps_ = {};
    super.connectedCallback();
    const urlPath = new URL(document.URL).pathname;
    if (urlPath.length <= 1) {
      return;
    }

    window.CrPolicyStrings = {
      controlledSettingPolicy:
          loadTimeData.getString('controlledSettingPolicy'),
    };

    const appId = urlPath.substring(1);
    BrowserProxy.getInstance().handler.getApp(appId).then((result) => {
      this.app_ = result.app;
    });

    BrowserProxy.getInstance().handler.getApps().then((result) => {
      for (const app of result.apps) {
        this.apps_[app.id] = app;
      }
    });

    // Listens to app update.
    const callbackRouter = BrowserProxy.getInstance().callbackRouter;
    callbackRouter.onAppChanged.addListener(this.onAppChanged_.bind(this));
    callbackRouter.onAppRemoved.addListener(this.onAppRemoved_.bind(this));
  }

  private onAppChanged_(app: App) {
    if (this.app_ && app.id === this.app_.id) {
      this.app_ = app;
    }
    this.apps_ = {...this.apps_, [app.id]: app};
  }

  private onAppRemoved_(appId: string) {
    delete this.apps_[appId];
    this.apps_ = {...this.apps_};
  }

  private getAppIcon_(app: App|null): string {
    return app ? getAppIcon(app) : '';
  }

  private appUnready_(app: App|null): boolean {
    return !app;
  }

  private getPermissionsHeader_(formattedOrigin: string): string {
    if (formattedOrigin) {
      return this.i18n(
          'appManagementPermissionsWithOriginLabel', formattedOrigin);
    } else {
      return this.i18n('appManagementPermissionsLabel');
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'web-app-settings-app': WebAppSettingsAppElement;
  }
}

customElements.define(WebAppSettingsAppElement.is, WebAppSettingsAppElement);
