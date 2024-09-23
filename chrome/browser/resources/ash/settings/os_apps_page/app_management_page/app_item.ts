// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import './app_management_cros_shared_style.css.js';
import './app_management_cros_shared_vars.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_icons.css.js';

import {App, AppType} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {AppManagementEntryPoint, AppManagementEntryPointsHistogramName} from 'chrome://resources/cr_components/app_management/constants.js';
import {getAppIcon} from 'chrome://resources/cr_components/app_management/util.js';
import {assertNotReached} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AppManagementStoreMixin} from '../../common/app_management/store_mixin.js';

import {getTemplate} from './app_item.html.js';
import {openAppDetailPage} from './util.js';

const AppManagementAppItemElementBase = AppManagementStoreMixin(PolymerElement);

export class AppManagementAppItemElement extends
    AppManagementAppItemElementBase {
  static get is() {
    return 'app-management-app-item';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      app: {
        type: Object,
      },
    };
  }

  app: App;

  override ready(): void {
    super.ready();

    this.addEventListener('click', this.onClick_);
  }

  private onClick_(): void {
    openAppDetailPage(this.app.id);
    chrome.metricsPrivate.recordEnumerationValue(
        AppManagementEntryPointsHistogramName,
        this.getAppManagementEntryPoint_(this.app.type),
        Object.keys(AppManagementEntryPoint).length);
  }

  private iconUrlFromId_(app: App): string {
    return getAppIcon(app);
  }

  private getAppManagementEntryPoint_(appType: AppType):
      AppManagementEntryPoint {
    switch (appType) {
      case AppType.kArc:
        return AppManagementEntryPoint.MAIN_VIEW_ARC;
      case AppType.kChromeApp:
      case AppType.kStandaloneBrowser:
      case AppType.kStandaloneBrowserChromeApp:
        // TODO(crbug.com/40188614): Figure out appropriate behavior for
        // Lacros-hosted chrome-apps.
        return AppManagementEntryPoint.MAIN_VIEW_CHROME_APP;
      case AppType.kWeb:
        return AppManagementEntryPoint.MAIN_VIEW_WEB_APP;
      case AppType.kPluginVm:
        return AppManagementEntryPoint.MAIN_VIEW_PLUGIN_VM;
      case AppType.kBorealis:
        return AppManagementEntryPoint.MAIN_VIEW_BOREALIS;
      default:
        assertNotReached();
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'app-management-app-item': AppManagementAppItemElement;
  }
}

customElements.define(
    AppManagementAppItemElement.is, AppManagementAppItemElement);
