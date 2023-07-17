// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/localized_link/localized_link.js';

import {App} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {getSubAppsOfSelectedApp} from 'chrome://resources/cr_components/app_management/util.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './permission_heading.html.js';
import {AppManagementStoreMixin} from './store_mixin.js';

const AppManagementPermissionHeadingElementBase =
    AppManagementStoreMixin(I18nMixin(PolymerElement));

export class AppManagementPermissionHeadingElement extends
    AppManagementPermissionHeadingElementBase {
  static get is() {
    return 'app-management-permission-heading';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      app: Object,
      hasSubApps_: Boolean,
    };
  }

  app: App;
  private hasSubApps_: boolean;

  override connectedCallback(): void {
    super.connectedCallback();
    this.watch(
        'hasSubApps_', state => getSubAppsOfSelectedApp(state).length > 0);
    this.updateFromStore();
  }

  private getParentAppPermissionExplanationString_(): string {
    return this.i18n(
        'appManagementParentAppPermissionExplanation',
        this.app.title ? this.app.title : '');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'app-management-permission-heading': AppManagementPermissionHeadingElement;
  }
}

customElements.define(
    AppManagementPermissionHeadingElement.is,
    AppManagementPermissionHeadingElement);
