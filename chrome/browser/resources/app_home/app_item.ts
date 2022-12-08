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
      data: Object,
    };
  }

  data: AppInfo;

  override ready() {
    super.ready();
    this.addEventListener('contextmenu', this.handleContextMenu_);
  }

  private handleContextMenu_(e: MouseEvent) {
    this.fire_('open-menu', {
      data: this.data,
      event: e,
    });

    e.preventDefault();
    e.stopPropagation();
  }

  private fire_(eventName: string, detail?: any) {
    this.dispatchEvent(
        new CustomEvent(eventName, {bubbles: true, composed: true, detail}));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'app-item': AppItemElement;
  }
}

customElements.define(AppItemElement.is, AppItemElement);