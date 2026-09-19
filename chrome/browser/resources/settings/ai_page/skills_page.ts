// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import '../controls/settings_toggle_button.js';
import '../settings_page/settings_subpage.js';
import '../icons.html.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';

import {SettingsViewMixinLit} from '../settings_page/settings_view_mixin_lit.js';

import {getCss} from './skills_page.css.js';
import {getHtml} from './skills_page.html.js';

const SettingsSkillsPageElementBase = SettingsViewMixinLit(CrLitElement);

export class SettingsSkillsPageElement extends SettingsSkillsPageElementBase {
  static get is() {
    return 'settings-skills-page';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  protected onSkillsGalleryLinkClick_() {
    OpenWindowProxyImpl.getInstance().openUrl('chrome://skills');
  }

  // SettingsViewMixinLit implementation.
  override focusBackButton() {
    this.shadowRoot.querySelector('settings-subpage')!.focusBackButton();
  }
}

export type SkillsPageElement = SettingsSkillsPageElement;

declare global {
  interface HTMLElementTagNameMap {
    'settings-skills-page': SettingsSkillsPageElement;
  }
}

customElements.define(SettingsSkillsPageElement.is, SettingsSkillsPageElement);
