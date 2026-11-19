// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../settings_page/settings_section.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {SettingsViewMixinLit} from '../settings_page/settings_view_mixin_lit.js';

import {getHtml} from './home_page.html.js';

const SettingsHomePageElementBase = SettingsViewMixinLit(CrLitElement);

export class SettingsHomePageElement extends SettingsHomePageElementBase {
  static get is() {
    return 'settings-home-page';
  }

  override render() {
    return getHtml.bind(this)();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-home-page': SettingsHomePageElement;
  }
}

customElements.define(SettingsHomePageElement.is, SettingsHomePageElement);
