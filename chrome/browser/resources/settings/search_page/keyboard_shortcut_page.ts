// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-keyboard-shortcut-page' is the settings page containing
 * the keyboard shortcut setting.
 */
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import '../settings_page/settings_section.js';
import '../settings_shared.css.js';
import '../controls/settings_dropdown_menu.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {DropdownMenuOptionList, SettingsDropdownMenuElement} from '../controls/settings_dropdown_menu.js';
import {loadTimeData} from '../i18n_setup.js';

import {getTemplate} from './keyboard_shortcut_page.html.js';
import {SearchEnginesBrowserProxyImpl, SearchEnginesInteractions} from './search_engines_browser_proxy.js';

export interface KeyboardShortcutPageElement {
  $: {
    dropdown: SettingsDropdownMenuElement,
  };
}

const KeyboardShortcutPageElementBase = PrefsMixin(PolymerElement);

export class KeyboardShortcutPageElement extends
    KeyboardShortcutPageElementBase {
  static get is() {
    return 'settings-keyboard-shortcut-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      keyboardShortcutMenuOptions_: {
        readOnly: true,
        type: Array,
        value() {
          return [
            {
              value: 'true',
              name: loadTimeData.getString(
                  'searchEnginesKeyboardShortcutsSpaceOrTab'),
            },
            {
              value: 'false',
              name: loadTimeData.getString('searchEnginesKeyboardShortcutsTab'),
            },
          ];
        },
      },
    };
  }

  declare private keyboardShortcutMenuOptions_: DropdownMenuOptionList;

  private onKeyboardShortcutSettingChange_() {
    const spaceEnabled =
        this.getPref('omnibox.keyword_space_triggering_enabled').value;

    SearchEnginesBrowserProxyImpl.getInstance()
        .recordSearchEnginesPageHistogram(
            spaceEnabled ?
                SearchEnginesInteractions.KEYBOARD_SHORTCUT_SPACE_OR_TAB :
                SearchEnginesInteractions.KEYBOARD_SHORTCUT_TAB);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-keyboard-shortcut-page': KeyboardShortcutPageElement;
  }
}

customElements.define(
    KeyboardShortcutPageElement.is, KeyboardShortcutPageElement);
