// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/localized_link/localized_link.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar.js';
import './strings.m.js';
import './app_content_item.js';
import './file_handling_item.js';
import './more_permissions_item.js';
import './run_on_os_login_item.js';
import './permission_item.js';
import './window_mode_item.js';
import './icons.html.js';
import './uninstall_button.js';
import './supported_links_item.js';
import './supported_links_overlapping_apps_dialog.js';
import './supported_links_dialog.js';

import type {App} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {BrowserProxy} from 'chrome://resources/cr_components/app_management/browser_proxy.js';
import type {AppMap} from 'chrome://resources/cr_components/app_management/constants.js';
import {getAppIcon} from 'chrome://resources/cr_components/app_management/util.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import {createDummyApp} from './web_app_settings_utils.js';

const AppElementBase = I18nMixinLit(CrLitElement);

// TODO(crbug.com/40213759): Investigate end-to-end WebAppSettings tests
export class AppElement extends AppElementBase {
  static get is() {
    return 'web-app-settings-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      app_: {type: Object},
      iconUrl_: {type: String},
      showSearch_: {type: Boolean},
      apps_: {type: Object},
    };
  }

  protected app_: App = createDummyApp();
  protected apps_: AppMap = {};
  protected iconUrl_: string = '';
  protected showSearch_: boolean = false;

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if ((changedProperties as Map<PropertyKey, unknown>).has('app_')) {
      this.iconUrl_ = getAppIcon(this.app_);
    }
  }

  override connectedCallback() {
    super.connectedCallback();
    this.apps_ = {};
    this.hidden = true;
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
      assert(result.app);
      this.app_ = result.app;
      this.hidden = false;
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
    if (app.id === this.app_.id) {
      this.app_ = app;
    }
    this.apps_ = {...this.apps_, [app.id]: app};
  }

  private onAppRemoved_(appId: string) {
    delete this.apps_[appId];
    this.apps_ = {...this.apps_};
  }

  protected getPermissionsHeader_(): string {
    return this.app_.formattedOrigin ?
        this.i18n(
            'appManagementPermissionsWithOriginLabel',
            this.app_.formattedOrigin) :
        this.i18n('appManagementPermissionsLabel');
  }

  protected getTitle_() {
    return this.app_.title || '';
  }

  protected shouldShowSystemNotificationsSettingsLink_(): boolean {
    return this.app_.showSystemNotificationsSettingsLink;
  }

  protected openNotificationsSystemSettings_(e: CustomEvent<{event: Event}>):
      void {
    // A place holder href with the value "#" is used to have a compliant link.
    // This prevents the browser from navigating the window to "#"
    e.detail.event.preventDefault();
    e.stopPropagation();
    // <if expr="is_macosx">
    BrowserProxy.getInstance().handler.openSystemNotificationSettings(
        this.app_.id);
    // </if>
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'web-app-settings-app': AppElement;
  }
}

customElements.define(AppElement.is, AppElement);
