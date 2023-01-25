// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AppInfo} from './app_home.mojom-webui.js';
import {getTemplate} from './app_item.html.js';

export class AppItemElement extends PolymerElement {
  static get is() {
    return 'app-item';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      appInfo: Object,
    };
  }

  appInfo: AppInfo;

  override ready() {
    super.ready();
    this.addEventListener('contextmenu', this.handleContextMenu_);
  }

  private handleContextMenu_(e: MouseEvent) {
    this.fire_('open-menu', {
      appInfo: this.appInfo,
      event: e,
    });

    e.preventDefault();
    e.stopPropagation();
  }

  private fire_(eventName: string, detail?: any) {
    this.dispatchEvent(
        new CustomEvent(eventName, {bubbles: true, composed: true, detail}));
  }

  private getIconUrl_() {
    const url = new URL(this.appInfo.iconUrl.url);
    // For web app, the backend serves grayscale image when the app is not
    // locally installed automatically and doesn't recognize this query param,
    // but we add a query param here to force browser to refetch the image.
    if (!this.appInfo.isLocallyInstalled) {
      url.searchParams.append('grayscale', 'true');
    }
    return url;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'app-item': AppItemElement;
  }
}

customElements.define(AppItemElement.is, AppItemElement);