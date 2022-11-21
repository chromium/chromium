// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './themes.html.js';

export interface ThemesElement {
  $: {
    backButton: HTMLButtonElement,
  };
}

export class ThemesElement extends PolymerElement {
  static get is() {
    return 'customize-chrome-themes';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {};
  }

  private onBackClick_() {
    this.dispatchEvent(new Event('back-click'));
  }

  private onSelectTheme_() {
    this.dispatchEvent(new Event('theme-select'));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'customize-chrome-themes': ThemesElement;
  }
}

customElements.define(ThemesElement.is, ThemesElement);
