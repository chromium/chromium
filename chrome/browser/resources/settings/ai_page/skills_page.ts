// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import '../controls/settings_toggle_button.js';
import '../settings_columned_section.css.js';
import '../settings_page/settings_subpage.js';
import '../settings_shared.css.js';
import '../icons.html.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';

import {getTemplate} from './skills_page.html.js';

const SettingsSkillsPageElementBase = PrefsMixin(PolymerElement);

export class SettingsSkillsPageElement extends SettingsSkillsPageElementBase {
  static get is() {
    return 'settings-skills-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      routePath: String,
    };
  }

  declare routePath: string;

  private onSkillsGalleryClick_() {
    OpenWindowProxyImpl.getInstance().openUrl('chrome://skills');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-skills-page': SettingsSkillsPageElement;
  }
}

customElements.define(SettingsSkillsPageElement.is, SettingsSkillsPageElement);
