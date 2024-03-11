// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../controls/settings_dropdown_menu.js';

import {MacroName} from 'chrome://resources/ash/common/accessibility/macro_names.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/ash/common/cr_elements/web_ui_listener_mixin.js';
import {PrefsMixin} from 'chrome://resources/cr_components/settings_prefs/prefs_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DeepLinkingMixin} from '../common/deep_linking_mixin.js';
import {RouteObserverMixin} from '../common/route_observer_mixin.js';
import {DropdownMenuOptionList} from '../controls/settings_dropdown_menu.js';
import {Route, routes} from '../router.js';

import {getTemplate} from './facegaze_facial_expression_subpage.html.js';

const SettingsFaceGazeFacialExpressionSubpageElementBase =
    DeepLinkingMixin(RouteObserverMixin(
        WebUiListenerMixin(PrefsMixin(I18nMixin(PolymerElement)))));

export interface SettingsFaceGazeFacialExpressionSubpageElement {
  $: {};
}

export class SettingsFaceGazeFacialExpressionSubpageElement extends
    SettingsFaceGazeFacialExpressionSubpageElementBase {
  static get is() {
    return 'settings-facegaze-facial-expression-subpage' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      leftClickMenuOptions_: {
        type: Array,
        value: () => [],
      },

      rightClickMenuOptions_: {
        type: Array,
        value: () => [],
      },

      resetCursorMenuOptions_: {
        type: Array,
        value: () => [],
      },

      toggleDictationMenuOptions_: {
        type: Array,
        value: () => [],
      },

      leftClickPref_: {
        type: Object,
        notify: true,
        observer: 'updateLeftClickPref_',
        value(): chrome.settingsPrivate.PrefObject {
          return {
            value: '',
            type: chrome.settingsPrivate.PrefType.STRING,
            key: 'MOUSE_CLICK_LEFT_pref',
          };
        },
      },

      rightClickPref_: {
        type: Object,
        notify: true,
        observer: 'updateRightClickPref_',
        value(): chrome.settingsPrivate.PrefObject {
          return {
            value: '',
            type: chrome.settingsPrivate.PrefType.STRING,
            key: 'MOUSE_CLICK_RIGHT_pref',
          };
        },
      },

      resetCursorPref_: {
        type: Object,
        notify: true,
        observer: 'updateResetCursorPref_',
        value(): chrome.settingsPrivate.PrefObject {
          return {
            value: '',
            type: chrome.settingsPrivate.PrefType.STRING,
            key: 'RESET_CURSOR_pref',
          };
        },
      },

      toggleDictationPref_: {
        type: Object,
        notify: true,
        observer: 'updateToggleDictationPref_',
        value(): chrome.settingsPrivate.PrefObject {
          return {
            value: '',
            type: chrome.settingsPrivate.PrefType.STRING,
            key: 'TOGGLE_DICTATION_pref',
          };
        },
      },
    };
  }

  static get observers() {
    return [
      'updateLeftClickPref_(leftClickPref_.*)',
      'updateRightClickPref_(rightClickPref_.*)',
      'updateResetCursorPref_(resetCursorPref_.*)',
      'updateToggleDictationPref_(toggleDictationPref_.*)',
    ];
  }

  private leftClickMenuOptions_: DropdownMenuOptionList;
  private rightClickMenuOptions_: DropdownMenuOptionList;
  private resetCursorMenuOptions_: DropdownMenuOptionList;
  private toggleDictationMenuOptions_: DropdownMenuOptionList;
  private leftClickPref_: chrome.settingsPrivate.PrefObject<string>;
  private rightClickPref_: chrome.settingsPrivate.PrefObject<string>;
  private resetCursorPref_: chrome.settingsPrivate.PrefObject<string>;
  private toggleDictationPref_: chrome.settingsPrivate.PrefObject<string>;

  override ready(): void {
    super.ready();

    this.leftClickMenuOptions_ = this.getGestureMenuOptions_();
    this.rightClickMenuOptions_ = this.getGestureMenuOptions_();
    this.resetCursorMenuOptions_ = this.getGestureMenuOptions_();
    this.toggleDictationMenuOptions_ = this.getGestureMenuOptions_();
    this.updateVirtualPrefs_();
  }

  override currentRouteChanged(route: Route): void {
    // Does not apply to this page.
    if (route !== routes.MANAGE_FACEGAZE_FACIAL_EXPRESSIONS_SETTINGS) {
      return;
    }

    this.attemptDeepLink();
  }

  // These values correspond to FacialGesture in
  // accessibility_common/facegaze/gesture_detector.ts, except "deselect".
  // TODO(b:322510392): Localize these strings.
  private getGestureMenuOptions_(): DropdownMenuOptionList {
    return [
      {value: 'deselect', name: '(none)', hidden: false},
      {value: 'browsDown', name: 'brows down', hidden: false},
      {value: 'browInnerUp', name: 'brow inner up', hidden: false},
      {value: 'jawOpen', name: 'jaw open', hidden: false},
      {value: 'mouthLeft', name: 'mouth left', hidden: false},
      {value: 'mouthRight', name: 'mouth right', hidden: false},
      {value: 'mouthPucker', name: 'mouth pucker', hidden: false},
      {value: 'eyesLookDown', name: 'eyes look down', hidden: false},
      {value: 'eyesLookUp', name: 'eyes look up', hidden: false},
    ];
  }

  private updateVirtualPrefs_(): void {
    const assignedGestures =
        this.get('prefs.settings.a11y.face_gaze.gestures_to_macros.value');

    // Build the reverse map.
    const macrosToGestures = new Map();
    for (const [gesture, macro] of Object.entries(assignedGestures)) {
      if (macro === '') {
        // Unassigned.
        continue;
      }
      if (!macrosToGestures.has(macro)) {
        macrosToGestures.set(macro, [gesture]);
      } else {
        macrosToGestures.get(macro).push(gesture);
      }
    }

    this.leftClickPref_ =
        this.updateVirtualPref_(macrosToGestures, MacroName.MOUSE_CLICK_LEFT);
    this.rightClickPref_ =
        this.updateVirtualPref_(macrosToGestures, MacroName.MOUSE_CLICK_RIGHT);
    this.resetCursorPref_ =
        this.updateVirtualPref_(macrosToGestures, MacroName.RESET_CURSOR);
    this.toggleDictationPref_ =
        this.updateVirtualPref_(macrosToGestures, MacroName.TOGGLE_DICTATION);
  }

  private updateVirtualPref_(
      macrosToGestures: Map<MacroName, string[]>,
      macro: MacroName): chrome.settingsPrivate.PrefObject {
    let value = 'deselect';
    if (macrosToGestures.has(macro)) {
      const mouseClickGestures = macrosToGestures.get(macro);
      if (mouseClickGestures && mouseClickGestures.length > 0) {
        value =
            mouseClickGestures[0];  // TODO(b:322510392): support multi-select.
      }
    }
    return {
      type: chrome.settingsPrivate.PrefType.STRING,
      key: macro + '_pref',
      value,
    };
  }

  private updateLeftClickPref_(): void {
    this.updateFromVirtualPref_(
        MacroName.MOUSE_CLICK_LEFT, this.leftClickPref_.value);
  }

  private updateRightClickPref_(): void {
    this.updateFromVirtualPref_(
        MacroName.MOUSE_CLICK_RIGHT, this.rightClickPref_.value);
  }

  private updateResetCursorPref_(): void {
    this.updateFromVirtualPref_(
        MacroName.RESET_CURSOR, this.resetCursorPref_.value);
  }

  private updateToggleDictationPref_(): void {
    this.updateFromVirtualPref_(
        MacroName.TOGGLE_DICTATION, this.toggleDictationPref_.value);
  }

  private updateFromVirtualPref_(macro: MacroName, value: string): void {
    if (value === '') {
      // Initializing.
      return;
    }

    const newLeftClickMenuOptions = Object.assign(this.leftClickMenuOptions_);
    const newRightClickMenuOptions = Object.assign(this.rightClickMenuOptions_);
    const newResetCursorMenuOptions =
        Object.assign(this.resetCursorMenuOptions_);
    const newToggleDictationMenuOptions =
        Object.assign(this.toggleDictationMenuOptions_);

    // First clear out the previous macro for this gesture.
    const assignedGestures =
        this.get('prefs.settings.a11y.face_gaze.gestures_to_macros.value');
    let alreadySet = false;
    for (const [currentGesture, assignedMacro] of Object.entries(
             assignedGestures)) {
      if (macro === assignedMacro) {
        if (currentGesture === value) {
          alreadySet = true;
        }
        this.setPrefDictEntry(
            'settings.a11y.face_gaze.gestures_to_macros', currentGesture,
            MacroName.UNSPECIFIED);

        // Make `currentGesture` visible in all the drop-downs.
        newLeftClickMenuOptions
            .find(
                (item: {value: string}) =>
                    item.value === currentGesture)!.hidden = false;
        newRightClickMenuOptions
            .find(
                (item: {value: string}) =>
                    item.value === currentGesture)!.hidden = false;
        newResetCursorMenuOptions
            .find(
                (item: {value: string}) =>
                    item.value === currentGesture)!.hidden = false;
        newToggleDictationMenuOptions
            .find(
                (item: {value: string}) =>
                    item.value === currentGesture)!.hidden = false;

        break;
      }
    }
    if (value !== 'deselect') {
      // Update the gesture->macro mapping pref.
      if (!alreadySet) {
        // TODO(b:322510392): Sometimes the prefs are updated (console.log the
        // pref dict gives the appropriate value) but listening for pref changes
        // in another Renderer context shows that the commit did not occur. This
        // seems to happen when we perform multiple updates to the dict
        // back-to-back. Is there some way to 'commit' this set to ensure it
        // goes through?
        this.setPrefDictEntry(
            'settings.a11y.face_gaze.gestures_to_macros', value, macro);
      }
      // Make 'gesture' hidden in all the other drop-downs.
      if (macro !== MacroName.MOUSE_CLICK_LEFT) {
        newLeftClickMenuOptions
            .find((item: {value: string}) => item.value === value)!.hidden =
            true;
      }
      if (macro !== MacroName.MOUSE_CLICK_RIGHT) {
        newRightClickMenuOptions
            .find((item: {value: string}) => item.value === value)!.hidden =
            true;
      }
      if (macro !== MacroName.RESET_CURSOR) {
        newResetCursorMenuOptions
            .find((item: {value: string}) => item.value === value)!.hidden =
            true;
      }
      if (macro !== MacroName.TOGGLE_DICTATION) {
        newToggleDictationMenuOptions
            .find((item: {value: string}) => item.value === value)!.hidden =
            true;
      }
    }

    // Force polymer to update the objects.
    // TODO(b:322510392): This isn't working consistently.
    this.leftClickMenuOptions_ = [];
    this.rightClickMenuOptions_ = [];
    this.resetCursorMenuOptions_ = [];
    this.toggleDictationMenuOptions_ = [];
    this.leftClickMenuOptions_ = newLeftClickMenuOptions;
    this.rightClickMenuOptions_ = newRightClickMenuOptions;
    this.resetCursorMenuOptions_ = newResetCursorMenuOptions;
    this.toggleDictationMenuOptions_ = newToggleDictationMenuOptions;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsFaceGazeFacialExpressionSubpageElement.is]:
        SettingsFaceGazeFacialExpressionSubpageElement;
  }
}

customElements.define(
    SettingsFaceGazeFacialExpressionSubpageElement.is,
    SettingsFaceGazeFacialExpressionSubpageElement);
