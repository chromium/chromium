// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'facegaze-actions-card' is the card element containing facegaze
 *  action settings.
 */

import '../controls/settings_dropdown_menu.js';
import '../os_settings_page/settings_card.js';
import '../settings_shared.css.js';
import '../os_settings_page/os_settings_animated_pages.js';
import 'chrome://resources/cros_components/chip/chip.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {FacialGesture} from 'chrome://resources/ash/common/accessibility/facial_gestures.js';
import {MacroName} from 'chrome://resources/ash/common/accessibility/macro_names.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/ash/common/cr_elements/web_ui_listener_mixin.js';
import {DomRepeat, DomRepeatEvent, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DeepLinkingMixin} from '../common/deep_linking_mixin.js';
import {RouteObserverMixin} from '../common/route_observer_mixin.js';
import {DropdownMenuOptionList} from '../controls/settings_dropdown_menu.js';
import {Route, routes} from '../router.js';

import {getTemplate} from './facegaze_actions_card.html.js';
import {FACE_GAZE_GESTURE_TO_MACROS_PREF, FACEGAZE_ACTION_ASSIGN_GESTURE_EVENT_NAME, FACEGAZE_COMMAND_PAIR_ADDED_EVENT_NAME, FaceGazeCommandPair, FaceGazeUtils} from './facegaze_constants.js';

const FaceGazeActionsCardElementBase = DeepLinkingMixin(RouteObserverMixin(
    WebUiListenerMixin(PrefsMixin(I18nMixin(PolymerElement)))));

export interface FaceGazeActionsCardElement {
  $: {
    recognitionConfidenceRepeat: DomRepeat,
  };
}

export class FaceGazeActionsCardElement extends FaceGazeActionsCardElementBase {
  static readonly FACEGAZE_COMMAND_PAIRS_PROPERTY_NAME =
      'commandPairs_' as const;
  private showAddActionDialog_: boolean;

  // This should be kept in sync with the pref with all interactions.
  private commandPairs_: FaceGazeCommandPair[] = [];

  static get is() {
    return 'facegaze-actions-card' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      commandPairs_: {
        type: Array,
        value: () => [],
      },
      showAddActionDialog_: {
        type: Boolean,
        value: false,
      },
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

      keyPressToggleOverviewMenuOptions_: {
        type: Array,
        value: () => [],
      },

      keyPressMediaPlayPauseMenuOptions_: {
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

      mouseLongClickLeftPref_: {
        type: Object,
        value(): chrome.settingsPrivate.PrefObject {
          return {
            value: '',
            type: chrome.settingsPrivate.PrefType.STRING,
            key: 'MOUSE_LONG_CLICK_LEFT_pref',
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

      keyPressToggleOverviewPref_: {
        type: Object,
        value(): chrome.settingsPrivate.PrefObject {
          return {
            value: '',
            type: chrome.settingsPrivate.PrefType.STRING,
            key: 'KEY_PRESS_TOGGLE_OVERVIEW_pref',
          };
        },
      },

      keyPressMediaPlayPausePref_: {
        type: Object,
        value(): chrome.settingsPrivate.PrefObject {
          return {
            value: '',
            type: chrome.settingsPrivate.PrefType.STRING,
            key: 'KEY_PRESS_MEDIA_PLAY_PAUSE_pref',
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
      'updateLongClickLeftPref_(longClickLeftPref_.*)',
      'updateResetCursorPref_(resetCursorPref_.*)',
      'updateToggleDictationPref_(toggleDictationPref_.*)',
      'updateKeyPressSpacePref_(keyPressSpacePref_.*)',
      'updateKeyPressDownPref_(keyPressDownPref_.*)',
      'updateKeyPressLeftPref_(keyPressLeftPref_.*)',
      'updateKeyPressRightPref_(keyPressRightPref_.*)',
      'updateKeyPressUpPref_(keyPressUpPref_.*)',
      'updateKeyPressToggleOverviewPref_(keyPressToggleOverviewPref_.*)',
      'updateKeyPressMediaPlayPausePref_(keyPressMediaPlayPausePref_.*)',
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
  private longClickLeftMenuOptions_: DropdownMenuOptionList;
  private resetCursorMenuOptions_: DropdownMenuOptionList;
  private toggleDictationMenuOptions_: DropdownMenuOptionList;
  private keyPressSpaceMenuOptions_: DropdownMenuOptionList;
  private keyPressDownMenuOptions_: DropdownMenuOptionList;
  private keyPressLeftMenuOptions_: DropdownMenuOptionList;
  private keyPressRightMenuOptions_: DropdownMenuOptionList;
  private keyPressUpMenuOptions_: DropdownMenuOptionList;
  private keyPressToggleOverviewMenuOptions_: DropdownMenuOptionList;
  private keyPressMediaPlayPauseMenuOptions_: DropdownMenuOptionList;
  private leftClickPref_: chrome.settingsPrivate.PrefObject<string>;
  private rightClickPref_: chrome.settingsPrivate.PrefObject<string>;
  private longClickLeftPref_: chrome.settingsPrivate.PrefObject<string>;
  private resetCursorPref_: chrome.settingsPrivate.PrefObject<string>;
  private toggleDictationPref_: chrome.settingsPrivate.PrefObject<string>;
  private keyPressSpacePref_: chrome.settingsPrivate.PrefObject<string>;
  private keyPressDownPref_: chrome.settingsPrivate.PrefObject<string>;
  private keyPressLeftPref_: chrome.settingsPrivate.PrefObject<string>;
  private keyPressRightPref_: chrome.settingsPrivate.PrefObject<string>;
  private keyPressUpPref_: chrome.settingsPrivate.PrefObject<string>;
  private keyPressToggleOverviewPref_:
      chrome.settingsPrivate.PrefObject<string>;
  private keyPressMediaPlayPausePref_:
      chrome.settingsPrivate.PrefObject<string>;
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
    this.longClickLeftMenuOptions_ = this.getGestureMenuOptions_();
    this.resetCursorMenuOptions_ = this.getGestureMenuOptions_();
    this.toggleDictationMenuOptions_ = this.getGestureMenuOptions_();
    this.keyPressSpaceMenuOptions_ = this.getGestureMenuOptions_();
    this.keyPressDownMenuOptions_ = this.getGestureMenuOptions_();
    this.keyPressLeftMenuOptions_ = this.getGestureMenuOptions_();
    this.keyPressRightMenuOptions_ = this.getGestureMenuOptions_();
    this.keyPressUpMenuOptions_ = this.getGestureMenuOptions_();
    this.keyPressToggleOverviewMenuOptions_ = this.getGestureMenuOptions_();
    this.keyPressMediaPlayPauseMenuOptions_ = this.getGestureMenuOptions_();
    this.updateVirtualPrefs_();

    this.updateConfiguredCommandPairsFromPrefs_();
  }

  override currentRouteChanged(route: Route): void {
    // Does not apply to this page.
    if (route !== routes.MANAGE_FACEGAZE_SETTINGS) {
      return;
    }

    this.attemptDeepLink();
  }

  private getCurrentAssignedGestures_(): Record<FacialGesture, MacroName> {
    return {...this.get(FACE_GAZE_GESTURE_TO_MACROS_PREF)} as
        Record<FacialGesture, MacroName>;
  }

  private onAddActionButtonClick_(): void {
    this.showAddActionDialog_ = true;
  }

  private onAddActionDialogClose_(): void {
    this.showAddActionDialog_ = false;
  }

  // These values correspond to FacialGesture in
  // accessibility_common/facegaze/gesture_detector.ts, except "deselect".
  // TODO(b:341770655): Localize these strings.
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
    const assignedGestures = this.getCurrentAssignedGestures_();

    // Build the reverse map.
    const macrosToGestures = new Map();
    for (const [gesture, macro] of Object.entries(assignedGestures)) {
      if (macro === MacroName.UNSPECIFIED) {
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
    this.longClickLeftPref_ = this.updateVirtualPref_(
        macrosToGestures, MacroName.MOUSE_LONG_CLICK_LEFT);
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
    this.keyPressToggleOverviewPref_ = this.updateVirtualPref_(
        macrosToGestures, MacroName.KEY_PRESS_TOGGLE_OVERVIEW);
    this.keyPressMediaPlayPausePref_ = this.updateVirtualPref_(
        macrosToGestures, MacroName.KEY_PRESS_MEDIA_PLAY_PAUSE);
  }

  private updateVirtualPref_(
      macrosToGestures: Map<MacroName, string[]>,
      macro: MacroName): chrome.settingsPrivate.PrefObject {
    let value = 'deselect';
    if (macrosToGestures.has(macro)) {
      const mouseClickGestures = macrosToGestures.get(macro);
      if (mouseClickGestures && mouseClickGestures.length > 0) {
        value =
            mouseClickGestures[0];  // TODO(b:341770655): support multi-select.
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

  private updateLongClickLeftPref_(): void {
    this.updateFromVirtualPref_(
        MacroName.MOUSE_LONG_CLICK_LEFT, this.longClickLeftPref_.value);
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

  private updateKeyPressToggleOverviewPref_(): void {
    this.updateFromVirtualPref_(
        MacroName.KEY_PRESS_TOGGLE_OVERVIEW,
        this.keyPressToggleOverviewPref_.value);
  }

  private updateKeyPressMediaPlayPausePref_(): void {
    this.updateFromVirtualPref_(
        MacroName.KEY_PRESS_MEDIA_PLAY_PAUSE,
        this.keyPressMediaPlayPausePref_.value);
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
    const newLongClickLeftMenuOptions =
        Object.assign(this.longClickLeftMenuOptions_);
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
    const newKeyPressToggleOverviewMenuOptions =
        Object.assign(this.keyPressToggleOverviewMenuOptions_);
    const newKeyPressMediaPlayPauseMenuOptions =
        Object.assign(this.keyPressMediaPlayPauseMenuOptions_);
    const newMenuOptions = [
      newLeftClickMenuOptions,
      newRightClickMenuOptions,
      newLongClickLeftMenuOptions,
      newResetCursorMenuOptions,
      newToggleDictationMenuOptions,
      newKeyPressSpaceMenuOptions,
      newKeyPressDownMenuOptions,
      newKeyPressLeftMenuOptions,
      newKeyPressRightMenuOptions,
      newKeyPressUpMenuOptions,
      newKeyPressToggleOverviewMenuOptions,
      newKeyPressMediaPlayPauseMenuOptions,
    ];
    const assignedGestures = this.getCurrentAssignedGestures_();

    // First clear out the previous macro for this gesture, if it has changed.
    let alreadySet = false;
    for (const [currentGesture, assignedMacro] of Object.entries(
             assignedGestures)) {
      if (macro === assignedMacro) {
        if (currentGesture === value) {
          alreadySet = true;
          break;
        }
        assignedGestures[currentGesture as FacialGesture] =
            MacroName.UNSPECIFIED;

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
        assignedGestures[value as FacialGesture] = macro;
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
      if (macro !== MacroName.MOUSE_LONG_CLICK_LEFT) {
        this.setDropdownMenuOptionsHiddenForGesture_(
            newLongClickLeftMenuOptions, value, true);
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
      if (macro !== MacroName.KEY_PRESS_TOGGLE_OVERVIEW) {
        this.setDropdownMenuOptionsHiddenForGesture_(
            newKeyPressToggleOverviewMenuOptions, value, true);
      }
      if (macro !== MacroName.KEY_PRESS_MEDIA_PLAY_PAUSE) {
        this.setDropdownMenuOptionsHiddenForGesture_(
            newKeyPressMediaPlayPauseMenuOptions, value, true);
      }
    }
    this.set(FACE_GAZE_GESTURE_TO_MACROS_PREF, {...assignedGestures});

    // Force polymer to update the objects.
    // TODO(b:341770655): This isn't working consistently.
    this.leftClickMenuOptions_ = [];
    this.rightClickMenuOptions_ = [];
    this.longClickLeftMenuOptions_ = [];
    this.resetCursorMenuOptions_ = [];
    this.toggleDictationMenuOptions_ = [];
    this.leftClickMenuOptions_ = newLeftClickMenuOptions;
    this.rightClickMenuOptions_ = newRightClickMenuOptions;
    this.longClickLeftMenuOptions_ = newLongClickLeftMenuOptions;
    this.resetCursorMenuOptions_ = newResetCursorMenuOptions;
    this.toggleDictationMenuOptions_ = newToggleDictationMenuOptions;
    this.keyPressSpaceMenuOptions_ = newKeyPressSpaceMenuOptions;
    this.keyPressDownMenuOptions_ = newKeyPressDownMenuOptions;
    this.keyPressLeftMenuOptions_ = newKeyPressLeftMenuOptions;
    this.keyPressRightMenuOptions_ = newKeyPressRightMenuOptions;
    this.keyPressUpMenuOptions_ = newKeyPressUpMenuOptions;
    this.keyPressToggleOverviewMenuOptions_ =
        newKeyPressToggleOverviewMenuOptions;
    this.keyPressMediaPlayPauseMenuOptions_ =
        newKeyPressMediaPlayPauseMenuOptions;
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

  private onAssignGestureButtonClick_(): void {
    // TODO(b:351025155): We need to send information to
    // facegaze_actions_add_dialog to tell it to set its current page, and send
    // the details of this action.
    this.showAddActionDialog_ = true;
  }

  private getActionDisplayText_(action: MacroName): string {
    return FaceGazeUtils.getMacroDisplayText(action);
  }

  private getGestureDisplayText_(gesture: FacialGesture|null): string {
    return FaceGazeUtils.getGestureDisplayText(gesture);
  }

  removeCommandPairForTest(macro: MacroName, gesture: FacialGesture): void {
    this.updatePrefWithRemovedCommandPair_(
        new FaceGazeCommandPair(macro, gesture));
  }

  // When an action is removed from the list, update the pref and then update
  // the UI accordingly.
  private onRemoveCommandPairButtonClick_(
      e: DomRepeatEvent<FaceGazeCommandPair>): void {
    const removedCommandPair: FaceGazeCommandPair = e.model.item;
    this.updatePrefWithRemovedCommandPair_(removedCommandPair);

    const removeIndex = this.commandPairs_.findIndex(
        (item: FaceGazeCommandPair) => item.equals(removedCommandPair));
    this.splice(
        FaceGazeActionsCardElement.FACEGAZE_COMMAND_PAIRS_PROPERTY_NAME,
        removeIndex, 1);
  }

  private updatePrefWithRemovedCommandPair_(removedCommandPair:
                                                FaceGazeCommandPair): void {
    // Get current assigned gestures to macros.
    const assignedGestures = this.getCurrentAssignedGestures_();

    for (const [currentGesture, assignedMacro] of Object.entries(
             assignedGestures)) {
      if (removedCommandPair.equals(new FaceGazeCommandPair(
              assignedMacro, currentGesture as FacialGesture))) {
        assignedGestures[currentGesture as FacialGesture] =
            MacroName.UNSPECIFIED;
        break;
      }
    }

    this.set(FACE_GAZE_GESTURE_TO_MACROS_PREF, assignedGestures);
  }

  addCommandPairForTest(macro: MacroName, gesture: FacialGesture): void {
    const commandPair = new FaceGazeCommandPair(macro, gesture);
    const event = new CustomEvent(FACEGAZE_COMMAND_PAIR_ADDED_EVENT_NAME, {
      bubbles: true,
      composed: true,
      detail: commandPair,
    });
    this.onCommandPairAdded_(event);
  }

  // When an action is added from the dialog, update the pref and then update
  // the UI accordingly.
  private onCommandPairAdded_(
      e: HTMLElementEventMap[typeof FACEGAZE_COMMAND_PAIR_ADDED_EVENT_NAME]):
      void {
    const newCommandPair = e.detail;
    // TODO(b:353403651): Ensure the gesture for the left-click action cannot
    // be unassigned as that would make FaceGaze unusable and the user may
    // require assistance in reassigning the action.
    this.updatePrefWithAddedCommandPair_(newCommandPair);

    // If gesture is already mapped to another action, remove that pairing as
    // gesture can only be mapped to one action.
    const unassignIndex = this.commandPairs_.findIndex(
        (item) => item.gesture === newCommandPair.gesture &&
            item.action !== newCommandPair.action);

    if (unassignIndex >= 0) {
      // Update configuration and notify Polymer.
      this.commandPairs_[unassignIndex].gesture = null;
      this.notifyPath(
          `${FaceGazeActionsCardElement.FACEGAZE_COMMAND_PAIRS_PROPERTY_NAME}.${
              unassignIndex}.gesture`);
    }

    // TODO(b:341770796): This logic will need to be updated once the gesture
    // threshold becomes part of the setting.
    // We will have to:
    // 1. Remove the gesture from the previously set action if there is one
    // (logic exists above)
    // 2. Check to see if there is a matching gesture/action pair for which we
    // need to update the gesture threshold.
    // If there is a matching pair, then update the existing item instead of
    // pushing a new one.
    const updateIndex = this.commandPairs_.findIndex(
        (item: FaceGazeCommandPair) => item.equals(newCommandPair));
    if (updateIndex < 0) {
      // Add new gesture/action pairing if it does not already exist.
      this.push(
          FaceGazeActionsCardElement.FACEGAZE_COMMAND_PAIRS_PROPERTY_NAME,
          newCommandPair);
    }
  }

  private updatePrefWithAddedCommandPair_(newCommandPair: FaceGazeCommandPair):
      void {
    if (!newCommandPair.gesture) {
      console.error(
          'FaceGaze added action with no valid gesture value: ' +
          this.getActionDisplayText_(newCommandPair.action));
      return;
    }

    // Get current assigned gestures to macros.
    const assignedGestures = this.getCurrentAssignedGestures_();

    // If the current gesture is already set to a macro, clear out the macro for
    // the gesture.
    let alreadySet = false;
    for (const [currentGesture, assignedMacro] of Object.entries(
             assignedGestures)) {
      if (newCommandPair.gesture === currentGesture) {
        if (newCommandPair.action === assignedMacro) {
          alreadySet = true;
          break;
        }

        assignedGestures[currentGesture] = MacroName.UNSPECIFIED;
        break;
      }
    }

    if (!alreadySet) {
      assignedGestures[newCommandPair.gesture] = newCommandPair.action;
    }

    this.set(FACE_GAZE_GESTURE_TO_MACROS_PREF, assignedGestures);
  }

  private updateConfiguredCommandPairsFromPrefs_(): void {
    // Get current assigned gestures to macros.
    const assignedGestures = this.getCurrentAssignedGestures_();

    for (const [currentGesture, assignedMacro] of Object.entries(
             assignedGestures)) {
      if (assignedMacro !== MacroName.UNSPECIFIED) {
        this.push(
            FaceGazeActionsCardElement.FACEGAZE_COMMAND_PAIRS_PROPERTY_NAME,
            new FaceGazeCommandPair(
                assignedMacro, currentGesture as FacialGesture));
      }
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [FaceGazeActionsCardElement.is]: FaceGazeActionsCardElement;
  }

  interface HTMLElementEventMap {
    [FACEGAZE_ACTION_ASSIGN_GESTURE_EVENT_NAME]: CustomEvent<MacroName>;
  }
}

customElements.define(
    FaceGazeActionsCardElement.is, FaceGazeActionsCardElement);
