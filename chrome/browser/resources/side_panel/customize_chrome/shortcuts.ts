// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CustomizeChromePageHandlerInterface} from './customize_chrome.mojom-webui.js';
import {CustomizeChromeApiProxy} from './customize_chrome_api_proxy.js';
import {getTemplate} from './shortcuts.html.js';


export interface ShortcutsElement {}

export class ShortcutsElement extends PolymerElement {
  static get is() {
    return 'customize-chrome-shortcuts';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      customLinksEnabled_: Boolean,
      shortcutsVisible_: Boolean,
    };
  }

  private customLinksEnabled_: boolean;
  private shortcutsVisible_: boolean;
  private pageHandler_: CustomizeChromePageHandlerInterface;

  override ready() {
    super.ready();
  }

  constructor() {
    super();
    const {handler} = CustomizeChromeApiProxy.getInstance();
    this.pageHandler_ = handler;
    this.pageHandler_.getMostVisitedSettings().then(
        ({customLinksEnabled, shortcutsVisible}) => {
          this.customLinksEnabled_ = customLinksEnabled;
          this.shortcutsVisible_ = shortcutsVisible;
        });
  }

  override connectedCallback() {
    super.connectedCallback();
  }

  apply() {
    this.pageHandler_.setMostVisitedSettings(
        this.customLinksEnabled_,
        /* shortcutsVisible= */ this.shortcutsVisible_);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'customize-chrome-shortcuts': ShortcutsElement;
  }
}

customElements.define(ShortcutsElement.is, ShortcutsElement);
