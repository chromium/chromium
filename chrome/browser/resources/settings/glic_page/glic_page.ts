// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import '../controls/settings_toggle_button.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';

import {getTemplate} from './glic_page.html.js';

export enum SettingsGlicPageFeaturePrefName {
  LAUNCHER_ENABLED = 'glic.launcher_enabled',
  // TODO(crbug.com/379166610): Keyboard shortcut
}

const SettingsGlicPageElementBase = PrefsMixin(PolymerElement);

export interface SettingsGlicPageElement {
  $: {
    launcherToggle: SettingsToggleButtonElement,
  };
}

export class SettingsGlicPageElement extends SettingsGlicPageElementBase {
  static get is() {
    return 'settings-glic-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      prefs: {
        type: Object,
        notify: true,
      },
    };
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-glic-page': SettingsGlicPageElement;
  }
}

customElements.define(SettingsGlicPageElement.is, SettingsGlicPageElement);
