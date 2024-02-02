// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './app_management_cros_shared_style.css.js';
import './app_content_dialog.js';
import '//resources/ash/common/cr_elements/cr_link_row/cr_link_row.js';

import {App} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app_content_item.html.js';

const AppManagementAppContentItemElementBase = I18nMixin(PolymerElement);
export class AppManagementAppContentItemElement extends
    AppManagementAppContentItemElementBase {
  static get is() {
    return 'app-management-app-content-item';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      app: Object,
      showAppContentDialog: {
        type: Boolean,
        value: false,
      },
      hidden: {
        type: Boolean,
        computed: 'isAppContentHidden_(app)',
        reflectToAttribute: true,
      },
    };
  }

  app: App;
  appContentLabel: string;
  appContentSublabel: string;
  showAppContentDialog: boolean;

  override ready(): void {
    super.ready();
    // Disable hover styles from cr-actionable-row-style because they do not
    // match the style of App Settings.
    this.shadowRoot!.querySelector('cr-link-row')!.toggleAttribute(
        'effectively-disabled_', true);
  }

  private onAppContentClick_(): void {
    this.showAppContentDialog = true;
  }

  private onAppContentDialogClose_(): void {
    this.showAppContentDialog = false;
  }

  // App Content section is hidden when there's no scope_extensions entries.
  private isAppContentHidden_(): boolean {
    return !this.app || !this.app.scopeExtensions ||
        !this.app.scopeExtensions.length;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'app-management-app-content-item': AppManagementAppContentItemElement;
  }
}

customElements.define(
    AppManagementAppContentItemElement.is, AppManagementAppContentItemElement);
