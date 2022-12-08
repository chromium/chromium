// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './colors.js';
import './theme_snapshot.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './appearance.html.js';

export interface AppearanceElement {
  $: {
    editThemeButton: HTMLButtonElement,
  };
}

export class AppearanceElement extends PolymerElement {
  static get is() {
    return 'customize-chrome-appearance';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {};
  }

  private onEditThemeClicked_() {
    this.dispatchEvent(new Event('edit-theme-click'));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'customize-chrome-appearance': AppearanceElement;
  }
}

customElements.define(AppearanceElement.is, AppearanceElement);
