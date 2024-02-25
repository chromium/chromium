// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/localized_link/localized_link.js';

import {App} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {getParentApp, getSubAppsOfSelectedApp} from 'chrome://resources/cr_components/app_management/util.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AppManagementStoreMixin} from '../../common/app_management/store_mixin.js';

import {getTemplate} from './permission_heading.html.js';
import {openAppDetailPage} from './util.js';

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
      parentApp_: Object,
      isSubApp_: {
        type: Boolean,
        computed: 'has_(parentApp_)',
      },
    };
  }

  app: App;
  private hasSubApps_: boolean;
  private parentApp_: App|null;
  private isSubApp_: boolean;

  override connectedCallback(): void {
    super.connectedCallback();
    this.watch(
        'hasSubApps_', state => getSubAppsOfSelectedApp(state).length > 0);
    this.watch('parentApp_', state => getParentApp(state));
    this.updateFromStore();
  }

  private has_(parentApp: Object): boolean {
    return !!parentApp;
  }

  private getParentAppPermissionExplanationString_(): string {
    if (this.hasSubApps_) {
      return this.i18n(
          'appManagementParentAppPermissionExplanation',
          String(this.app.title));
    }
    return '';
  }

  private getSubAppPermissionExplanationString_(): TrustedHTML {
    if (this.parentApp_) {
      return this.i18nAdvanced(
          'appManagementSubAppPermissionExplanation',
          {substitutions: [String(this.parentApp_.title)]});
    }
    assert(window.trustedTypes);
    return window.trustedTypes.emptyHTML;
  }

  private onManagePermissionsClicked_(event: CustomEvent<{event: Event}>):
      void {
    event.detail.event.preventDefault();
    if (this.parentApp_) {
      openAppDetailPage(this.parentApp_.id);
    }
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
