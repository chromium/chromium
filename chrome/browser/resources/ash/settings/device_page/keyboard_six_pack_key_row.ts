// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'keyboard-six-pack-key-row' displays a six pack key alongside a dropdown
 * menu that allows users to set the shortcut that triggers the corresponding
 * six pack key action.
 */

import '/shared/settings/prefs/prefs.js';
import './input_device_settings_shared.css.js';
import '../icons.html.js';
import '../settings_shared.css.js';
import '../controls/settings_dropdown_menu.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';

import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DropdownMenuOptionList} from '../controls/settings_dropdown_menu.js';

import {SixPackKey, SixPackShortcutModifier} from './input_device_settings_types.js';
import {getTemplate} from './keyboard_six_pack_key_row.html.js';

interface SixPackKeyProperties {
  menuOptions: DropdownMenuOptionList;
  label: string;
}

const disabledMenuOption = {
  value: SixPackShortcutModifier.kNone,
  name: loadTimeData.getString('sixPackKeyDisabled'),
};

export const sixPackKeyProperties: {[k in SixPackKey]: SixPackKeyProperties} = {
  [SixPackKey.DELETE]: {
    menuOptions: [
      {
        value: SixPackShortcutModifier.kAlt,
        name: loadTimeData.getString('sixPackKeyDeleteAlt'),
      },
      {
        value: SixPackShortcutModifier.kSearch,
        name: loadTimeData.getString('sixPackKeyDeleteSearch'),
      },
      disabledMenuOption,
    ],
    label: loadTimeData.getString('sixPackKeyLabelDelete'),
  },
  [SixPackKey.HOME]: {
    menuOptions: [
      {
        value: SixPackShortcutModifier.kAlt,
        name: loadTimeData.getString('sixPackKeyHomeAlt'),
      },
      {
        value: SixPackShortcutModifier.kSearch,
        name: loadTimeData.getString('sixPackKeyHomeSearch'),
      },
      disabledMenuOption,
    ],
    label: loadTimeData.getString('sixPackKeyLabelHome'),
  },
  [SixPackKey.END]: {
    menuOptions: [
      {
        value: SixPackShortcutModifier.kAlt,
        name: loadTimeData.getString('sixPackKeyEndAlt'),
      },
      {
        value: SixPackShortcutModifier.kSearch,
        name: loadTimeData.getString('sixPackKeyEndSearch'),
      },
      disabledMenuOption,
    ],
    label: loadTimeData.getString('sixPackKeyLabelEnd'),
  },
  [SixPackKey.INSERT]: {
    menuOptions: [
      {
        value: SixPackShortcutModifier.kSearch,
        name: loadTimeData.getString('sixPackKeyInsertSearch'),
      },
      disabledMenuOption,
    ],
    label: loadTimeData.getString('sixPackKeyLabelInsert'),
  },
  [SixPackKey.PAGE_DOWN]: {
    menuOptions: [
      {
        value: SixPackShortcutModifier.kAlt,
        name: loadTimeData.getString('sixPackKeyPageDownAlt'),
      },
      {
        value: SixPackShortcutModifier.kSearch,
        name: loadTimeData.getString('sixPackKeyPageDownSearch'),
      },
      disabledMenuOption,
    ],
    label: loadTimeData.getString('sixPackKeyLabelPageDown'),
  },
  [SixPackKey.PAGE_UP]: {
    menuOptions: [
      {
        value: SixPackShortcutModifier.kAlt,
        name: loadTimeData.getString('sixPackKeyPageUpAlt'),
      },
      {
        value: SixPackShortcutModifier.kSearch,
        name: loadTimeData.getString('sixPackKeyPageUpSearch'),
      },
      disabledMenuOption,
    ],
    label: loadTimeData.getString('sixPackKeyLabelPageUp'),
  },
};

export class KeyboardSixPackKeyRowElement extends PolymerElement {
  static get is() {
    return 'keyboard-six-pack-key-row' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      key: {
        type: String,
      },
      modifier: {type: Number},
      pref: {
        type: Object,
      },

      keyLabel: {
        type: String,
        computed: 'computeKeyLabel(key)',
      },
    };
  }

  key: SixPackKey;
  modifier: SixPackShortcutModifier;
  pref: chrome.settingsPrivate.PrefObject;
  keyLabel: string;

  protected computeMenuOptions(): DropdownMenuOptionList {
    assert(this.key in sixPackKeyProperties);
    return sixPackKeyProperties[this.key].menuOptions;
  }

  protected computeKeyLabel(): string {
    assert(this.key in sixPackKeyProperties);
    return sixPackKeyProperties[this.key].label;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [KeyboardSixPackKeyRowElement.is]: KeyboardSixPackKeyRowElement;
  }
}

customElements.define(
    KeyboardSixPackKeyRowElement.is, KeyboardSixPackKeyRowElement);
