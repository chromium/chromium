// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../controls/settings_dropdown_menu.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {MacroName} from 'chrome://resources/ash/common/accessibility/macro_names.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/ash/common/cr_elements/web_ui_listener_mixin.js';
import {DomRepeat, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DeepLinkingMixin} from '../common/deep_linking_mixin.js';
import {RouteObserverMixin} from '../common/route_observer_mixin.js';
import {DropdownMenuOptionList} from '../controls/settings_dropdown_menu.js';
import {Route, routes} from '../router.js';

import {getTemplate} from './facegaze_facial_expression_subpage.html.js';

const SettingsFaceGazeFacialExpressionSubpageElementBase =
    DeepLinkingMixin(RouteObserverMixin(
        WebUiListenerMixin(PrefsMixin(I18nMixin(PolymerElement)))));

export interface SettingsFaceGazeFacialExpressionSubpageElement {
  $: {
    recognitionConfidenceRepeat: DomRepeat,
  };
}

/**
 * The facial gestures that are supported by FaceGaze.
 * These are copied from facegaze/gesture_detector.ts and if
 * these values get changed, those should too.
 * TODO(b:322510392): Share with gesture_detector.ts.
 */
enum FacialGesture {
  BROWS_DOWN = 'browsDown',
  BROW_INNER_UP = 'browInnerUp',
  EYES_LOOK_DOWN = 'eyesLookDown',
  EYES_LOOK_UP = 'eyesLookUp',
  JAW_OPEN = 'jawOpen',
  MOUTH_LEFT = 'mouthLeft',
  MOUTH_PUCKER = 'mouthPucker',
  MOUTH_RIGHT = 'mouthRight',
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
        value(): chrome.settingsPrivate.PrefObject {
          return {
            value: '',
            type: chrome.settingsPrivate.PrefType.STRING,
            key: 'TOGGLE_DICTATION_pref',
          };
        },
      },

      browsDownPref_: {
        type: Object,
        notify: true,
        computed: `getGestureToConfidencePref_('${FacialGesture.BROWS_DOWN}')`,
      },

      browInnerUpPref_: {
        type: Object,
        notify: true,
        computed:
            `getGestureToConfidencePref_('${FacialGesture.BROW_INNER_UP}')`,
      },

      eyesLookDownPref_: {
        type: Object,
        notify: true,
        computed:
            `getGestureToConfidencePref_('${FacialGesture.EYES_LOOK_DOWN}')`,
      },

      eyesLookUpPref_: {
        type: Object,
        notify: true,
        computed:
            `getGestureToConfidencePref_('${FacialGesture.EYES_LOOK_UP}')`,
      },

      jawOpenPref_: {
        type: Object,
        notify: true,
        computed: `getGestureToConfidencePref_('${FacialGesture.JAW_OPEN}')`,
      },

      mouthLeftPref_: {
        type: Object,
        notify: true,
        computed: `getGestureToConfidencePref_('${FacialGesture.MOUTH_LEFT}')`,
      },

      mouthRightPref_: {
        type: Object,
        notify: true,
        computed: `getGestureToConfidencePref_('${FacialGesture.MOUTH_RIGHT}')`,
      },

      mouthPuckerPref_: {
        type: Object,
        notify: true,
        computed:
            `getGestureToConfidencePref_('${FacialGesture.MOUTH_PUCKER}')`,
      },
    };
  }

  static get observers() {
    return [
      'updateLeftClickPref_(leftClickPref_.*)',
      'updateRightClickPref_(rightClickPref_.*)',
      'updateResetCursorPref_(resetCursorPref_.*)',
      'updateToggleDictationPref_(toggleDictationPref_.*)',
      'updateBrowsDownPref_(browsDownPref_.*)',
      'updateBrowInnerUpPref_(browInnerUpPref_.*)',
      'updateEyesLookDownPref_(eyesLookDownPref_.*)',
      'updateEyesLookUpPref_(eyesLookUpPref_.*)',
      'updateJawOpenPref_(jawOpenPref_.*)',
      'updateMouthLeftPref_(mouthLeftPref_.*)',
      'updateMouthRightPref_(mouthRightPref_.*)',
      'updateMouthPuckerPref_(mouthPuckerPref_.*)',
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
  private browsDownPref_: chrome.settingsPrivate.PrefObject<number>;
  private browInnerUpPref_: chrome.settingsPrivate.PrefObject<number>;
  private eyesLookDownPref_: chrome.settingsPrivate.PrefObject<number>;
  private eyesLookUpPref_: chrome.settingsPrivate.PrefObject<number>;
  private jawOpenPref_: chrome.settingsPrivate.PrefObject<number>;
  private mouthLeftPref_: chrome.settingsPrivate.PrefObject<number>;
  private mouthRightPref_: chrome.settingsPrivate.PrefObject<number>;
  private mouthPuckerPref_: chrome.settingsPrivate.PrefObject<number>;

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
      {value: FacialGesture.BROWS_DOWN, name: 'brows down', hidden: false},
      {
        value: FacialGesture.BROW_INNER_UP,
        name: 'brow inner up',
        hidden: false,
      },
      {value: FacialGesture.JAW_OPEN, name: 'jaw open', hidden: false},
      {value: FacialGesture.MOUTH_LEFT, name: 'mouth left', hidden: false},
      {value: FacialGesture.MOUTH_RIGHT, name: 'mouth right', hidden: false},
      {value: FacialGesture.MOUTH_PUCKER, name: 'mouth pucker', hidden: false},
      {
        value: FacialGesture.EYES_LOOK_DOWN,
        name: 'eyes look down',
        hidden: false,
      },
      {value: FacialGesture.EYES_LOOK_UP, name: 'eyes look up', hidden: false},
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
    const assignedGestures = {
        ...this.get('prefs.settings.a11y.face_gaze.gestures_to_macros.value')};

    // First clear out the previous macro for this gesture, if it has changed.
    let alreadySet = false;
    for (const [currentGesture, assignedMacro] of Object.entries(
             assignedGestures)) {
      if (macro === assignedMacro) {
        if (currentGesture === value) {
          alreadySet = true;
          break;
        }
        assignedGestures[currentGesture] = MacroName.UNSPECIFIED;

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
        assignedGestures[value] = macro;
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
      this.set(
          'prefs.settings.a11y.face_gaze.gestures_to_macros.value',
          {...assignedGestures});
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

  private getGestureToConfidencePref_(gestureName: FacialGesture):
      chrome.settingsPrivate.PrefObject<number> {
    const gesturesToConfidence =
        this.get('prefs.settings.a11y.face_gaze.gestures_to_confidence.value');
    let confidence = 60;  // Default.
    if (gestureName in gesturesToConfidence) {
      confidence = gesturesToConfidence[gestureName];
    }
    return {
      value: confidence,
      type: chrome.settingsPrivate.PrefType.NUMBER,
      key: 'CONFIDENCE_PREF_' + gestureName,
    };
  }

  private updateBrowsDownPref_(): void {
    this.updateGesturesToConfidencePref(
        FacialGesture.BROWS_DOWN, this.browsDownPref_.value);
  }

  private updateBrowInnerUpPref_(): void {
    this.updateGesturesToConfidencePref(
        FacialGesture.BROW_INNER_UP, this.browInnerUpPref_.value);
  }

  private updateEyesLookDownPref_(): void {
    this.updateGesturesToConfidencePref(
        FacialGesture.EYES_LOOK_DOWN, this.eyesLookDownPref_.value);
  }

  private updateEyesLookUpPref_(): void {
    this.updateGesturesToConfidencePref(
        FacialGesture.EYES_LOOK_UP, this.eyesLookUpPref_.value);
  }

  private updateJawOpenPref_(): void {
    this.updateGesturesToConfidencePref(
        FacialGesture.JAW_OPEN, this.jawOpenPref_.value);
  }

  private updateMouthLeftPref_(): void {
    this.updateGesturesToConfidencePref(
        FacialGesture.MOUTH_LEFT, this.mouthLeftPref_.value);
  }

  private updateMouthRightPref_(): void {
    this.updateGesturesToConfidencePref(
        FacialGesture.MOUTH_RIGHT, this.mouthRightPref_.value);
  }

  private updateMouthPuckerPref_(): void {
    this.updateGesturesToConfidencePref(
        FacialGesture.MOUTH_PUCKER, this.mouthPuckerPref_.value);
  }

  private updateGesturesToConfidencePref(
      gestureName: string, confidence: number): void {
    this.setPrefDictEntry(
        'settings.a11y.face_gaze.gestures_to_confidence', gestureName,
        confidence);
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
