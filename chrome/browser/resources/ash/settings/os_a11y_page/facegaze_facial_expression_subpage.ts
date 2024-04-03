// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../controls/settings_dropdown_menu.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {FacialGesture} from 'chrome://resources/ash/common/accessibility/facial_gestures.js';
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

      keyPressSpaceMenuOptions_: {
        type: Array,
        value: () => [],
      },

      keyPressDownMenuOptions_: {
        type: Array,
        value: () => [],
      },

      keyPressLeftMenuOptions_: {
        type: Array,
        value: () => [],
      },

      keyPressRightMenuOptions_: {
        type: Array,
        value: () => [],
      },

      keyPressUpMenuOptions_: {
        type: Array,
        value: () => [],
      },

      leftClickPref_: {
        type: Object,
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
        value(): chrome.settingsPrivate.PrefObject {
          return {
            value: '',
            type: chrome.settingsPrivate.PrefType.STRING,
            key: 'TOGGLE_DICTATION_pref',
          };
        },
      },

      keyPressSpacePref_: {
        type: Object,
        value(): chrome.settingsPrivate.PrefObject {
          return {
            value: '',
            type: chrome.settingsPrivate.PrefType.STRING,
            key: 'KEY_PRESS_SPACE_pref',
          };
        },
      },

      keyPressDownPref_: {
        type: Object,
        value(): chrome.settingsPrivate.PrefObject {
          return {
            value: '',
            type: chrome.settingsPrivate.PrefType.STRING,
            key: 'KEY_PRESS_DOWN_pref',
          };
        },
      },

      keyPressLeftPref_: {
        type: Object,
        value(): chrome.settingsPrivate.PrefObject {
          return {
            value: '',
            type: chrome.settingsPrivate.PrefType.STRING,
            key: 'KEY_PRESS_LEFT_pref',
          };
        },
      },

      keyPressRightPref_: {
        type: Object,
        value(): chrome.settingsPrivate.PrefObject {
          return {
            value: '',
            type: chrome.settingsPrivate.PrefType.STRING,
            key: 'KEY_PRESS_RIGHT_pref',
          };
        },
      },

      keyPressUpPref_: {
        type: Object,
        value(): chrome.settingsPrivate.PrefObject {
          return {
            value: '',
            type: chrome.settingsPrivate.PrefType.STRING,
            key: 'KEY_PRESS_UP_pref',
          };
        },
      },

      browInnerUpPref_: {
        type: Object,
        computed:
            `getGestureToConfidencePref_('${FacialGesture.BROW_INNER_UP}')`,
      },

      browsDownPref_: {
        type: Object,
        computed: `getGestureToConfidencePref_('${FacialGesture.BROWS_DOWN}')`,
      },

      eyeSquintLeftPref_: {
        type: Object,
        computed:
            `getGestureToConfidencePref_('${FacialGesture.EYE_SQUINT_LEFT}')`,
      },

      eyeSquintRightPref_: {
        type: Object,
        computed:
            `getGestureToConfidencePref_('${FacialGesture.EYE_SQUINT_RIGHT}')`,
      },

      eyesBlinkPref_: {
        type: Object,
        computed: `getGestureToConfidencePref_('${FacialGesture.EYES_BLINK}')`,
      },

      eyesLookDownPref_: {
        type: Object,
        computed:
            `getGestureToConfidencePref_('${FacialGesture.EYES_LOOK_DOWN}')`,
      },

      eyesLookLeftPref_: {
        type: Object,
        computed:
            `getGestureToConfidencePref_('${FacialGesture.EYES_LOOK_LEFT}')`,
      },

      eyesLookRightPref_: {
        type: Object,
        computed:
            `getGestureToConfidencePref_('${FacialGesture.EYES_LOOK_RIGHT}')`,
      },

      eyesLookUpPref_: {
        type: Object,
        computed:
            `getGestureToConfidencePref_('${FacialGesture.EYES_LOOK_UP}')`,
      },

      jawOpenPref_: {
        type: Object,
        computed: `getGestureToConfidencePref_('${FacialGesture.JAW_OPEN}')`,
      },

      mouthLeftPref_: {
        type: Object,
        computed: `getGestureToConfidencePref_('${FacialGesture.MOUTH_LEFT}')`,
      },

      mouthPuckerPref_: {
        type: Object,
        computed:
            `getGestureToConfidencePref_('${FacialGesture.MOUTH_PUCKER}')`,
      },

      mouthRightPref_: {
        type: Object,
        computed: `getGestureToConfidencePref_('${FacialGesture.MOUTH_RIGHT}')`,
      },

      mouthSmilePref_: {
        type: Object,
        computed: `getGestureToConfidencePref_('${FacialGesture.MOUTH_SMILE}')`,
      },

      mouthUpperUpPref_: {
        type: Object,
        computed:
            `getGestureToConfidencePref_('${FacialGesture.MOUTH_UPPER_UP}')`,
      },
    };
  }

  static get observers() {
    return [
      'updateLeftClickPref_(leftClickPref_.*)',
      'updateRightClickPref_(rightClickPref_.*)',
      'updateResetCursorPref_(resetCursorPref_.*)',
      'updateToggleDictationPref_(toggleDictationPref_.*)',
      'updateKeyPressSpacePref_(keyPressSpacePref_.*)',
      'updateKeyPressDownPref_(keyPressDownPref_.*)',
      'updateKeyPressLeftPref_(keyPressLeftPref_.*)',
      'updateKeyPressRightPref_(keyPressRightPref_.*)',
      'updateKeyPressUpPref_(keyPressUpPref_.*)',
      'updateBrowInnerUpPref_(browInnerUpPref_.*)',
      'updateBrowsDownPref_(browsDownPref_.*)',
      'updateEyeSquintLeftPref_(eyeSquintLeftPref_.*)',
      'updateEyeSquintRightPref_(eyeSquintRightPref_.*)',
      'updateEyesBlinkPref_(eyesBlinkPref_.*)',
      'updateEyesLookDownPref_(eyesLookDownPref_.*)',
      'updateEyesLookLeftPref_(eyesLookLeftPref_.*)',
      'updateEyesLookRightPref_(eyesLookRightPref_.*)',
      'updateEyesLookUpPref_(eyesLookUpPref_.*)',
      'updateJawOpenPref_(jawOpenPref_.*)',
      'updateMouthLeftPref_(mouthLeftPref_.*)',
      'updateMouthPuckerPref_(mouthPuckerPref_.*)',
      'updateMouthRightPref_(mouthRightPref_.*)',
      'updateMouthSmilePref_(mouthSmilePref_.*)',
      'updateMouthUpperUpPref_(mouthUpperUpPref_.*)',
    ];
  }

  private leftClickMenuOptions_: DropdownMenuOptionList;
  private rightClickMenuOptions_: DropdownMenuOptionList;
  private resetCursorMenuOptions_: DropdownMenuOptionList;
  private toggleDictationMenuOptions_: DropdownMenuOptionList;
  private keyPressSpaceMenuOptions_: DropdownMenuOptionList;
  private keyPressDownMenuOptions_: DropdownMenuOptionList;
  private keyPressLeftMenuOptions_: DropdownMenuOptionList;
  private keyPressRightMenuOptions_: DropdownMenuOptionList;
  private keyPressUpMenuOptions_: DropdownMenuOptionList;
  private leftClickPref_: chrome.settingsPrivate.PrefObject<string>;
  private rightClickPref_: chrome.settingsPrivate.PrefObject<string>;
  private resetCursorPref_: chrome.settingsPrivate.PrefObject<string>;
  private toggleDictationPref_: chrome.settingsPrivate.PrefObject<string>;
  private keyPressSpacePref_: chrome.settingsPrivate.PrefObject<string>;
  private keyPressDownPref_: chrome.settingsPrivate.PrefObject<string>;
  private keyPressLeftPref_: chrome.settingsPrivate.PrefObject<string>;
  private keyPressRightPref_: chrome.settingsPrivate.PrefObject<string>;
  private keyPressUpPref_: chrome.settingsPrivate.PrefObject<string>;
  private browInnerUpPref_: chrome.settingsPrivate.PrefObject<number>;
  private browsDownPref_: chrome.settingsPrivate.PrefObject<number>;
  private eyeSquintLeftPref_: chrome.settingsPrivate.PrefObject<number>;
  private eyeSquintRightPref_: chrome.settingsPrivate.PrefObject<number>;
  private eyesBlinkPref_: chrome.settingsPrivate.PrefObject<number>;
  private eyesLookDownPref_: chrome.settingsPrivate.PrefObject<number>;
  private eyesLookLeftPref_: chrome.settingsPrivate.PrefObject<number>;
  private eyesLookRightPref_: chrome.settingsPrivate.PrefObject<number>;
  private eyesLookUpPref_: chrome.settingsPrivate.PrefObject<number>;
  private jawOpenPref_: chrome.settingsPrivate.PrefObject<number>;
  private mouthLeftPref_: chrome.settingsPrivate.PrefObject<number>;
  private mouthRightPref_: chrome.settingsPrivate.PrefObject<number>;
  private mouthPuckerPref_: chrome.settingsPrivate.PrefObject<number>;
  private mouthSmilePref_: chrome.settingsPrivate.PrefObject<number>;
  private mouthUpperUpPref_: chrome.settingsPrivate.PrefObject<number>;

  override ready(): void {
    super.ready();

    this.leftClickMenuOptions_ = this.getGestureMenuOptions_();
    this.rightClickMenuOptions_ = this.getGestureMenuOptions_();
    this.resetCursorMenuOptions_ = this.getGestureMenuOptions_();
    this.toggleDictationMenuOptions_ = this.getGestureMenuOptions_();
    this.keyPressSpaceMenuOptions_ = this.getGestureMenuOptions_();
    this.keyPressDownMenuOptions_ = this.getGestureMenuOptions_();
    this.keyPressLeftMenuOptions_ = this.getGestureMenuOptions_();
    this.keyPressRightMenuOptions_ = this.getGestureMenuOptions_();
    this.keyPressUpMenuOptions_ = this.getGestureMenuOptions_();
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
      {
        value: FacialGesture.BROW_INNER_UP,
        name: 'brow inner up',
        hidden: false,
      },
      {value: FacialGesture.BROWS_DOWN, name: 'brows down', hidden: false},
      {
        value: FacialGesture.EYE_SQUINT_LEFT,
        name: 'squint left eye',
        hidden: false,
      },
      {
        value: FacialGesture.EYE_SQUINT_RIGHT,
        name: 'squint right eye',
        hidden: false,
      },
      {
        value: FacialGesture.EYES_BLINK,
        name: 'eyes blink',
        hidden: false,
      },
      {
        value: FacialGesture.EYES_LOOK_DOWN,
        name: 'eyes look down',
        hidden: false,
      },
      {
        value: FacialGesture.EYES_LOOK_LEFT,
        name: 'eyes look left',
        hidden: false,
      },
      {
        value: FacialGesture.EYES_LOOK_RIGHT,
        name: 'eyes look right',
        hidden: false,
      },
      {value: FacialGesture.EYES_LOOK_UP, name: 'eyes look up', hidden: false},
      {value: FacialGesture.JAW_OPEN, name: 'jaw open', hidden: false},
      {value: FacialGesture.MOUTH_LEFT, name: 'mouth left', hidden: false},
      {value: FacialGesture.MOUTH_PUCKER, name: 'mouth pucker', hidden: false},
      {value: FacialGesture.MOUTH_RIGHT, name: 'mouth right', hidden: false},
      {value: FacialGesture.MOUTH_SMILE, name: 'mouth smile', hidden: false},
      {
        value: FacialGesture.MOUTH_UPPER_UP,
        name: 'mouth upper up',
        hidden: false,
      },
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
    this.keyPressSpacePref_ =
        this.updateVirtualPref_(macrosToGestures, MacroName.KEY_PRESS_SPACE);
    this.keyPressDownPref_ =
        this.updateVirtualPref_(macrosToGestures, MacroName.KEY_PRESS_DOWN);
    this.keyPressLeftPref_ =
        this.updateVirtualPref_(macrosToGestures, MacroName.KEY_PRESS_LEFT);
    this.keyPressRightPref_ =
        this.updateVirtualPref_(macrosToGestures, MacroName.KEY_PRESS_RIGHT);
    this.keyPressUpPref_ =
        this.updateVirtualPref_(macrosToGestures, MacroName.KEY_PRESS_UP);
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

  private updateKeyPressSpacePref_(): void {
    this.updateFromVirtualPref_(
        MacroName.KEY_PRESS_SPACE, this.keyPressSpacePref_.value);
  }

  private updateKeyPressDownPref_(): void {
    this.updateFromVirtualPref_(
        MacroName.KEY_PRESS_DOWN, this.keyPressDownPref_.value);
  }

  private updateKeyPressLeftPref_(): void {
    this.updateFromVirtualPref_(
        MacroName.KEY_PRESS_LEFT, this.keyPressLeftPref_.value);
  }

  private updateKeyPressRightPref_(): void {
    this.updateFromVirtualPref_(
        MacroName.KEY_PRESS_RIGHT, this.keyPressRightPref_.value);
  }

  private updateKeyPressUpPref_(): void {
    this.updateFromVirtualPref_(
        MacroName.KEY_PRESS_UP, this.keyPressUpPref_.value);
  }

  private setDropdownMenuOptionsHiddenForGesture_(
      menuOptions: DropdownMenuOptionList, gesture: string,
      hidden: boolean): void {
    const menuOption = menuOptions.find(
        (item: {value: number|string}) => item.value === gesture);
    if (menuOption) {
      menuOption.hidden = hidden;
    }
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
    const newKeyPressSpaceMenuOptions =
        Object.assign(this.keyPressSpaceMenuOptions_);
    const newKeyPressDownMenuOptions =
        Object.assign(this.keyPressDownMenuOptions_);
    const newKeyPressLeftMenuOptions =
        Object.assign(this.keyPressLeftMenuOptions_);
    const newKeyPressRightMenuOptions =
        Object.assign(this.keyPressRightMenuOptions_);
    const newKeyPressUpMenuOptions = Object.assign(this.keyPressUpMenuOptions_);
    const newMenuOptions = [
      newLeftClickMenuOptions,
      newRightClickMenuOptions,
      newResetCursorMenuOptions,
      newToggleDictationMenuOptions,
      newKeyPressSpaceMenuOptions,
      newKeyPressDownMenuOptions,
      newKeyPressLeftMenuOptions,
      newKeyPressRightMenuOptions,
      newKeyPressUpMenuOptions,
    ];
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
        newMenuOptions.forEach((menuOptions) => {
          this.setDropdownMenuOptionsHiddenForGesture_(
              menuOptions, currentGesture, false);
        });
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
        this.setDropdownMenuOptionsHiddenForGesture_(
            newLeftClickMenuOptions, value, true);
      }
      if (macro !== MacroName.MOUSE_CLICK_RIGHT) {
        this.setDropdownMenuOptionsHiddenForGesture_(
            newRightClickMenuOptions, value, true);
      }
      if (macro !== MacroName.RESET_CURSOR) {
        this.setDropdownMenuOptionsHiddenForGesture_(
            newResetCursorMenuOptions, value, true);
      }
      if (macro !== MacroName.TOGGLE_DICTATION) {
        this.setDropdownMenuOptionsHiddenForGesture_(
            newToggleDictationMenuOptions, value, true);
      }
      if (macro !== MacroName.KEY_PRESS_SPACE) {
        this.setDropdownMenuOptionsHiddenForGesture_(
            newKeyPressSpaceMenuOptions, value, true);
      }
      if (macro !== MacroName.KEY_PRESS_DOWN) {
        this.setDropdownMenuOptionsHiddenForGesture_(
            newKeyPressDownMenuOptions, value, true);
      }
      if (macro !== MacroName.KEY_PRESS_LEFT) {
        this.setDropdownMenuOptionsHiddenForGesture_(
            newKeyPressLeftMenuOptions, value, true);
      }
      if (macro !== MacroName.KEY_PRESS_RIGHT) {
        this.setDropdownMenuOptionsHiddenForGesture_(
            newKeyPressRightMenuOptions, value, true);
      }
      if (macro !== MacroName.KEY_PRESS_UP) {
        this.setDropdownMenuOptionsHiddenForGesture_(
            newKeyPressUpMenuOptions, value, true);
      }
    }
    this.set(
        'prefs.settings.a11y.face_gaze.gestures_to_macros.value',
        {...assignedGestures});

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
    this.keyPressSpaceMenuOptions_ = newKeyPressSpaceMenuOptions;
    this.keyPressDownMenuOptions_ = newKeyPressDownMenuOptions;
    this.keyPressLeftMenuOptions_ = newKeyPressLeftMenuOptions;
    this.keyPressRightMenuOptions_ = newKeyPressRightMenuOptions;
    this.keyPressUpMenuOptions_ = newKeyPressUpMenuOptions;
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

  private updateBrowInnerUpPref_(): void {
    this.updateGesturesToConfidencePref(
        FacialGesture.BROW_INNER_UP, this.browInnerUpPref_.value);
  }

  private updateBrowsDownPref_(): void {
    this.updateGesturesToConfidencePref(
        FacialGesture.BROWS_DOWN, this.browsDownPref_.value);
  }

  private updateEyeSquintLeftPref_(): void {
    this.updateGesturesToConfidencePref(
        FacialGesture.EYE_SQUINT_LEFT, this.eyeSquintLeftPref_.value);
  }

  private updateEyeSquintRightPref_(): void {
    this.updateGesturesToConfidencePref(
        FacialGesture.EYE_SQUINT_RIGHT, this.eyeSquintRightPref_.value);
  }

  private updateEyesBlinkPref_(): void {
    this.updateGesturesToConfidencePref(
        FacialGesture.EYES_BLINK, this.eyesBlinkPref_.value);
  }

  private updateEyesLookDownPref_(): void {
    this.updateGesturesToConfidencePref(
        FacialGesture.EYES_LOOK_DOWN, this.eyesLookDownPref_.value);
  }

  private updateEyesLookLeftPref_(): void {
    this.updateGesturesToConfidencePref(
        FacialGesture.EYES_LOOK_LEFT, this.eyesLookLeftPref_.value);
  }

  private updateEyesLookRightPref_(): void {
    this.updateGesturesToConfidencePref(
        FacialGesture.EYES_LOOK_RIGHT, this.eyesLookRightPref_.value);
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

  private updateMouthPuckerPref_(): void {
    this.updateGesturesToConfidencePref(
        FacialGesture.MOUTH_PUCKER, this.mouthPuckerPref_.value);
  }

  private updateMouthRightPref_(): void {
    this.updateGesturesToConfidencePref(
        FacialGesture.MOUTH_RIGHT, this.mouthRightPref_.value);
  }

  private updateMouthSmilePref_(): void {
    this.updateGesturesToConfidencePref(
        FacialGesture.MOUTH_SMILE, this.mouthSmilePref_.value);
  }

  private updateMouthUpperUpPref_(): void {
    this.updateGesturesToConfidencePref(
        FacialGesture.MOUTH_UPPER_UP, this.mouthUpperUpPref_.value);
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
