// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import './shared_style.css.js';
import './prefs/pref_toggle_button.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PasswordManagerImpl} from './password_manager_proxy.js';
import {PrefToggleButtonElement} from './prefs/pref_toggle_button.js';
import {getTemplate} from './settings_section.html.js';

export interface SettingsSectionElement {
  $: {
    autosigninToggle: PrefToggleButtonElement,
    passwordToggle: PrefToggleButtonElement,
  };
}

export class SettingsSectionElement extends PolymerElement {
  static get is() {
    return 'settings-section';
  }

  static get template() {
    return getTemplate();
  }

  private onAddShortcutClick_() {
    // TODO(crbug.com/1358448): Record metrics on all entry points usage.
    // TODO(crbug.com/1358448): Hide the button for users after the shortcut is
    // installed.
    PasswordManagerImpl.getInstance().showAddShortcutDialog();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-section': SettingsSectionElement;
  }
}

customElements.define(SettingsSectionElement.is, SettingsSectionElement);
