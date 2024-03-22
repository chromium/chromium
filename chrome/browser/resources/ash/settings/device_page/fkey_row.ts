// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'fkey-row' displays an fKey alongside a dropdown menu that allows users to
 * set a shortcut for remapping key events to F11/F12.
 */

import '/shared/settings/prefs/prefs.js';
import './input_device_settings_shared.css.js';
import '../settings_shared.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';

import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {RouteObserverMixin} from '../common/route_observer_mixin.js';
import {DropdownMenuOptionList} from '../controls/settings_dropdown_menu.js';
import {Route, routes} from '../router.js';

import {getTemplate} from './fkey_row.html.js';
import {ExtendedFkeysModifier, Fkey, Keyboard, TopRowActionKey} from './input_device_settings_types.js';

function getTopRowActionKeyString(topRowActionKey: TopRowActionKey): string {
  switch (topRowActionKey) {
    case TopRowActionKey.kBack:
      return loadTimeData.getString('backKeyLabel');
    case TopRowActionKey.kForward:
      return loadTimeData.getString('forwardKeyLabel');
    case TopRowActionKey.kRefresh:
      return loadTimeData.getString('refreshKeyLabel');
    case TopRowActionKey.kFullscreen:
      return loadTimeData.getString('fullscreenKeyLabel');
    case TopRowActionKey.kOverview:
      return loadTimeData.getString('overviewKeyLabel');
    case TopRowActionKey.kScreenshot:
      return loadTimeData.getString('screenshotKeyLabel');
    case TopRowActionKey.kScreenBrightnessDown:
      return loadTimeData.getString('screenBrightnessDownKeyLabel');
    case TopRowActionKey.kScreenBrightnessUp:
      return loadTimeData.getString('screenBrightnessUpKeyLabel');
    case TopRowActionKey.kMicrophoneMute:
      return loadTimeData.getString('microphoneMuteKeyLabel');
    case TopRowActionKey.kVolumeMute:
      return loadTimeData.getString('muteKeyLabel');
    case TopRowActionKey.kVolumeDown:
      return loadTimeData.getString('volumeDownKeyLabel');
    case TopRowActionKey.kVolumeUp:
      return loadTimeData.getString('volumeUpKeyLabel');
    case TopRowActionKey.kKeyboardBacklightToggle:
      return loadTimeData.getString('backlightToggleKeyLabel');
    case TopRowActionKey.kKeyboardBacklightDown:
      return loadTimeData.getString('backlightDownKeyLabel');
    case TopRowActionKey.kKeyboardBacklightUp:
      return loadTimeData.getString('backlightUpKeyLabel');
    case TopRowActionKey.kNextTrack:
      return loadTimeData.getString('trackNextKeyLabel');
    case TopRowActionKey.kPreviousTrack:
      return loadTimeData.getString('trackPreviousKeyLabel');
    case TopRowActionKey.kPlayPause:
      return loadTimeData.getString('playPauseKeyLabel');
    case TopRowActionKey.kAllApplications:
      return loadTimeData.getString('allApplicationsKeyLabel');
    case TopRowActionKey.kEmojiPicker:
      return loadTimeData.getString('emojiPickerKeyLabel');
    case TopRowActionKey.kDictation:
      return loadTimeData.getString('dicationKeyLabel');
    case TopRowActionKey.kPrivacyScreenToggle:
      return loadTimeData.getString('privacyScreenToggleKeyLabel');
    case TopRowActionKey.kNone:
    case TopRowActionKey.kUnknown:
      return '';
    default:
      assertNotReached();
  }
}

const fKeyLabels = {
  [Fkey.F11]: loadTimeData.getString('f11KeyLabel'),
  [Fkey.F12]: loadTimeData.getString('f12KeyLabel'),
};

const FkeyRowElementBase = RouteObserverMixin(I18nMixin(PolymerElement));

export class FkeyRowElement extends FkeyRowElementBase {
  static get is() {
    return 'fkey-row' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      key: {type: String},

      keyLabel: {
        type: String,
        computed: 'computeKeyLabel(key)',
      },

      pref: {
        type: Object,
      },

      keyboard: {
        type: Object,
      },

      shortcutOptions: {
        type: Array,
      },
    };
  }

  key: Fkey;
  keyLabel: string;
  pref: chrome.settingsPrivate.PrefObject;
  keyboard: Keyboard;
  shortcutOptions: DropdownMenuOptionList;

  override currentRouteChanged(route: Route): void {
    // Does not apply to this page.
    if (route !== routes.PER_DEVICE_KEYBOARD_REMAP_KEYS) {
      return;
    }

    this.shortcutOptions = this.getMenuOptions();
  }

  getTopRowKeyLabel(): string {
    // F11 shortcuts include the key in the F1 position which corresponds
    // to the 1st entry in our `topRowActionKeys` array.
    const fkeyIndex = this.key === Fkey.F11 ? 0 : 1;
    assert(this.keyboard.topRowActionKeys);
    return getTopRowActionKeyString(this.keyboard.topRowActionKeys[fkeyIndex]);
  }

  private computeKeyLabel(): string {
    assert(this.key in fKeyLabels);
    return fKeyLabels[this.key];
  }

  private getFkeyShortcutOptions(): DropdownMenuOptionList {
    const topRowKeyLabel = this.getTopRowKeyLabel();
    const messageIdSuffix =
        this.keyboard.settings.topRowAreFkeys ? '' : 'Search';
    return [
      {
        value: ExtendedFkeysModifier.kShift,
        name: this.i18n(`fKeyShiftOption${messageIdSuffix}`, topRowKeyLabel),
      },
      {
        value: ExtendedFkeysModifier.kCtrlShift,
        name:
            this.i18n(`fKeyCtrlShiftOption${messageIdSuffix}`, topRowKeyLabel),
      },
      {
        value: ExtendedFkeysModifier.kAlt,
        name: this.i18n(`fKeyAltOption${messageIdSuffix}`, topRowKeyLabel),
      },
    ];
  }

  protected getMenuOptions(): DropdownMenuOptionList {
    return [
      {
        value: ExtendedFkeysModifier.kDisabled,
        name: this.i18n('perDeviceKeyboardKeyDisabled'),
      },
      ...this.getFkeyShortcutOptions(),
    ];
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [FkeyRowElement.is]: FkeyRowElement;
  }
}

customElements.define(FkeyRowElement.is, FkeyRowElement);
