// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './app_content_dialog.js';
import '//resources/cr_elements/cr_link_row/cr_link_row.js';

import type {App} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app_content_item.css.js';
import {getHtml} from './app_content_item.html.js';
import {createDummyApp} from './web_app_settings_utils.js';

export class AppContentItemElement extends CrLitElement {
  static get is() {
    return 'app-management-app-content-item';
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
      showAppContentDialog: {type: Boolean},
      hidden: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  app: App = createDummyApp();
  appContentLabel: string = '';
  appContentSublabel: string = '';
  showAppContentDialog: boolean = false;
  override hidden: boolean = false;

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('app')) {
      this.hidden = this.isAppContentHidden_();
    }
  }

  override firstUpdated() {
    // Disable hover styles from cr-actionable-row-style because they do not
    // match the style of App Settings.
    this.shadowRoot!.querySelector('cr-link-row')!.toggleAttribute(
        'effectively-disabled_', true);
  }

  protected onAppContentClick_(): void {
    this.showAppContentDialog = true;
  }

  protected onAppContentDialogClose_(): void {
    this.showAppContentDialog = false;
  }

  // App Content section is hidden when there's no scope_extensions entries.
  private isAppContentHidden_(): boolean {
    return !this.app.scopeExtensions || !this.app.scopeExtensions.length;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'app-management-app-content-item': AppContentItemElement;
  }
}

customElements.define(AppContentItemElement.is, AppContentItemElement);
