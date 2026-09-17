// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../controls/settings_toggle_button.js';
import '../settings_page/settings_subpage.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {SettingsViewMixinLit} from '../settings_page/settings_view_mixin_lit.js';

import {getCss} from './geic_subpage.css.js';
import {getHtml} from './geic_subpage.html.js';

const SettingsGeicSubpageElementBase = SettingsViewMixinLit(CrLitElement);

export class SettingsGeicSubpageElement extends SettingsGeicSubpageElementBase {
  static get is() {
    return 'settings-geic-subpage';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  // SettingsViewMixin implementation.
  override focusBackButton() {
    const subpage = this.shadowRoot.querySelector('settings-subpage');
    if (subpage) {
      subpage.focusBackButton();
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-geic-subpage': SettingsGeicSubpageElement;
  }
}

customElements.define(
    SettingsGeicSubpageElement.is, SettingsGeicSubpageElement);
