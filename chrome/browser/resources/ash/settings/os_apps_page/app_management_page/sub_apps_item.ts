// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/localized_link/localized_link.js';

import {App} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {getSubAppsOfSelectedApp} from 'chrome://resources/cr_components/app_management/util.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AppManagementStoreMixin} from '../../common/app_management/store_mixin.js';

import {getTemplate} from './sub_apps_item.html.js';

const AppManagementSubAppsItemElementBase =
    AppManagementStoreMixin(I18nMixin(PolymerElement));

export class AppManagementSubAppsItemElement extends
    AppManagementSubAppsItemElementBase {
  static get is() {
    return 'app-management-sub-apps-item';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      parentApp: Object,

      hidden: {
        type: Boolean,
        computed: 'isHidden_(subApps)',
        reflectToAttribute: true,
      },

      subApps: Object,
    };
  }

  parentApp: App;
  override hidden: boolean;
  subApps: App[];

  override connectedCallback(): void {
    super.connectedCallback();

    this.watch('subApps', state => getSubAppsOfSelectedApp(state));

    this.updateFromStore();
  }

  private getListHeadingString_(): string {
    return this.i18n(
        'appManagementSubAppsListHeading',
        this.parentApp.title ? this.parentApp.title : '');
  }

  /**
   * The sub app item is not available when an app has no sub apps.
   */
  private isHidden_(subApps: App[]): boolean {
    return !Array.isArray(subApps) || subApps.length === 0;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'app-management-sub-apps-item': AppManagementSubAppsItemElement;
  }
}

customElements.define(
    AppManagementSubAppsItemElement.is, AppManagementSubAppsItemElement);
