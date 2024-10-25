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
import './facegaze_icons.html.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import type {FacialGesture} from 'chrome://resources/ash/common/accessibility/facial_gestures.js';
import {MacroName} from 'chrome://resources/ash/common/accessibility/macro_names.js';
import type {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/ash/common/cr_elements/web_ui_listener_mixin.js';
import type {DomRepeatEvent} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DeepLinkingMixin} from '../common/deep_linking_mixin.js';
import {RouteObserverMixin} from '../common/route_observer_mixin.js';
import type {Route} from '../router.js';
import {routes} from '../router.js';

import {AddDialogPage} from './facegaze_actions_add_dialog.js';
import {getTemplate} from './facegaze_actions_card.html.js';
import type {FACEGAZE_ACTION_ASSIGN_GESTURE_EVENT_NAME, FACEGAZE_COMMAND_PAIR_ADDED_EVENT_NAME} from './facegaze_constants.js';
import {AssignedKeyCombo, FACE_GAZE_GESTURE_TO_KEY_COMBO_PREF, FACE_GAZE_GESTURE_TO_KEY_COMBO_PREF_DICT, FACE_GAZE_GESTURE_TO_MACROS_PREF, FaceGazeCommandPair, FaceGazeUtils} from './facegaze_constants.js';

const FaceGazeActionsCardElementBase = DeepLinkingMixin(RouteObserverMixin(
    WebUiListenerMixin(PrefsMixin(I18nMixin(PolymerElement)))));

export interface FaceGazeActionsCardElement {
  $: {};
}

export class FaceGazeActionsCardElement extends FaceGazeActionsCardElementBase {
  static readonly FACEGAZE_COMMAND_PAIRS_PROPERTY_NAME =
      'commandPairs_' as const;
  disabled: boolean;

  private showAddActionDialog_: boolean;
  private leftClickGestures_: FacialGesture[] = [];
  private dialogPageToShow_: AddDialogPage;
  private commandPairToConfigure_: FaceGazeCommandPair|null = null;
  private faceGazeActionsAlert_ = '';

  // This field stores the current state of gestures assigned to macros and
  // custom key combinations.
  private commandPairs_: FaceGazeCommandPair[] = [];

  static get is() {
    return 'facegaze-actions-card' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      disabled: {
        type: Boolean,
      },

      disableConfigureControls_: {
        type: Boolean,
        computed:
            'shouldDisableConfigureControls_(disabled, prefs.settings.a11y.face_gaze.actions_enabled.value)',
      },

      commandPairs_: {
        type: Array,
        value: () => [],
      },

      showAddActionDialog_: {
        type: Boolean,
        value: false,
      },

      leftClickGestures_: {
        type: Array,
        value: () => [],
      },

      dialogPageToShow_: {
        type: Number,
      },

      commandPairToConfigure_: {
        type: Object,
      },

      shouldAnnounceA11yActionFeedback_: {
        type: Boolean,
        computed: 'shouldAnnounceAlert_(faceGazeActionsAlert_)',
      },

      faceGazeActionsAlert_: {
        type: String,
      },
    };
  }

  static get observers() {
    return [`initFromPrefs_(prefs.settings.a11y.face_gaze.enabled.value)`];
  }

  private shouldAnnounceAlert_(): boolean {
    return this.faceGazeActionsAlert_ !== '';
  }

  override ready(): void {
    super.ready();
    this.initFromPrefs_();
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

  private getCurrentKeyCombos_(): Record<FacialGesture, string> {
    return {...this.get(FACE_GAZE_GESTURE_TO_KEY_COMBO_PREF)} as
        Record<FacialGesture, string>;
  }

  private shouldDisableConfigureControls_(): boolean {
    return this.disabled ||
        !this.getPref('settings.a11y.face_gaze.actions_enabled').value;
  }

  private onAddActionButtonClick_(): void {
    this.dialogPageToShow_ = AddDialogPage.SELECT_ACTION;
    this.leftClickGestures_ = this.computeLeftClickGestures_();
    this.showAddActionDialog_ = true;
    this.faceGazeActionsAlert_ = '';
  }

  private onAddActionDialogClose_(): void {
    this.showAddActionDialog_ = false;
    this.commandPairToConfigure_ = null;
  }

  private onConfigureGestureButtonClick_(
      e: DomRepeatEvent<FaceGazeCommandPair>): void {
    this.dialogPageToShow_ = AddDialogPage.GESTURE_THRESHOLD;
    this.commandPairToConfigure_ = e.model.item;
    this.showAddActionDialog_ = true;
  }

  private onAssignGestureButtonClick_(e: DomRepeatEvent<FaceGazeCommandPair>):
      void {
    this.dialogPageToShow_ = AddDialogPage.SELECT_GESTURE;
    this.leftClickGestures_ = this.computeLeftClickGestures_();
    this.commandPairToConfigure_ = e.model.item;
    this.showAddActionDialog_ = true;
    this.faceGazeActionsAlert_ = '';
  }

  private getActionDisplayText_(action: MacroName): string {
    return this.i18n(FaceGazeUtils.getMacroDisplayTextName(action));
  }

  private getSubLabel_(action: MacroName): string|null {
    const name = FaceGazeUtils.getMacroDisplaySubLabelName(action);
    return name ? this.i18n(name) : null;
  }

  private getGestureDisplayText_(gesture: FacialGesture|null): string {
    return this.i18n(FaceGazeUtils.getGestureDisplayTextName(gesture));
  }

  private getGestureIconName_(gesture: FacialGesture|null): string {
    return `facegaze:${FaceGazeUtils.getGestureIconName(gesture)}`;
  }

  private getKeyComboDisplayText_(commandPair: FaceGazeCommandPair): string
      |null {
    if (commandPair.action !== MacroName.CUSTOM_KEY_COMBINATION) {
      return null;
    }

    if (!commandPair.assignedKeyCombo) {
      console.error(this.getKeyComboErrorMessage_(commandPair.gesture));
      return null;
    }

    const keyCombo = commandPair.assignedKeyCombo.keyCombo;
    return this.i18n(
        'faceGazeMacroLabelAssignedCustomKeyCombo',
        FaceGazeUtils.getKeyComboDisplayText(keyCombo));
  }

  private getRemoveButtonLabel_(macro: MacroName): string {
    return this.i18n(
        'faceGazeActionsRemoveActionLabel', this.getActionDisplayText_(macro));
  }

  // When an action is removed from the list, update the pref and then update
  // the UI accordingly.
  private onRemoveCommandPairButtonClick_(
      e: DomRepeatEvent<FaceGazeCommandPair>): void {
    this.faceGazeActionsAlert_ = '';
    const removedCommandPair: FaceGazeCommandPair = e.model.item;
    this.removeCommandPairFromPref_(removedCommandPair);

    const removeCommandPairIndex = this.commandPairs_.findIndex(
        (item: FaceGazeCommandPair) => item.equals(removedCommandPair));
    this.splice(
        FaceGazeActionsCardElement.FACEGAZE_COMMAND_PAIRS_PROPERTY_NAME,
        removeCommandPairIndex, 1);

    this.faceGazeActionsAlert_ = this.i18n(
        'faceGazeActionsRemovedActionAlert',
        this.i18n(
            FaceGazeUtils.getMacroDisplayTextName(removedCommandPair.action)));

    // If there is one, set focus to the remove button of the next command pair.
    // Otherwise, set focus to the action button.
    if (this.commandPairs_[removeCommandPairIndex]) {
      const commandPairElements =
          this.shadowRoot!.querySelectorAll<HTMLElement>('.command-pair');
      const nextRemoveButton =
          commandPairElements[removeCommandPairIndex]
              .querySelector<CrButtonElement>('.icon-clear');
      nextRemoveButton!.focus();
    } else {
      const addActionButton =
          this.shadowRoot!.querySelector<CrButtonElement>('#addActionButton');
      addActionButton!.focus();
    }
  }

  private removeCommandPairFromPref_(removedCommandPair: FaceGazeCommandPair):
      void {
    const assignedGestures = this.getCurrentAssignedGestures_();

    for (const [currentGesture, assignedMacro] of Object.entries(
             assignedGestures)) {
      if (assignedMacro === removedCommandPair.action &&
          currentGesture === removedCommandPair.gesture) {
        delete assignedGestures[currentGesture as FacialGesture];
        break;
      }
    }

    this.set(FACE_GAZE_GESTURE_TO_MACROS_PREF, assignedGestures);

    if (removedCommandPair.action === MacroName.CUSTOM_KEY_COMBINATION &&
        removedCommandPair.gesture) {
      this.removeKeyComboFromPref_(
          removedCommandPair.gesture, removedCommandPair.assignedKeyCombo);
    }
  }

  // When an action is added from the dialog, update the pref and then update
  // the UI accordingly.
  private onCommandPairAdded_(
      e: HTMLElementEventMap[typeof FACEGAZE_COMMAND_PAIR_ADDED_EVENT_NAME]):
      void {
    const newCommandPair = e.detail;
    this.addCommandPairToPref_(newCommandPair);

    // If gesture is already mapped to another action, remove that pairing as
    // gesture can only be mapped to one action.
    const unassignIndex = this.commandPairs_.findIndex((item) => {
      return item.gesture === newCommandPair.gesture &&
          !item.actionsEqual(newCommandPair);
    });

    if (unassignIndex >= 0) {
      const unassignGesture = this.commandPairs_[unassignIndex].gesture;
      this.updateCommandPairGesture_(unassignIndex, null);

      // Unassign key combo after gesture is unassigned so FaceGaze does not
      // attempt to execute macro for gesture.
      if (this.commandPairs_[unassignIndex].action ===
              MacroName.CUSTOM_KEY_COMBINATION &&
          unassignGesture) {
        this.removeKeyComboFromPref_(
            unassignGesture,
            this.commandPairs_[unassignIndex].assignedKeyCombo);
      }
    }

    if (this.dialogPageToShow_ === AddDialogPage.SELECT_GESTURE) {
      // Update an existing row for the action if coming from the Assign a
      // Gesture page.
      const updateIndex = this.commandPairs_.findIndex(
          (item: FaceGazeCommandPair) =>
              item.actionsEqual(newCommandPair) && item.gesture === null);
      if (updateIndex > -1) {
        this.updateCommandPairGesture_(updateIndex, newCommandPair.gesture);
      }
    } else {
      const updateIndex = this.commandPairs_.findIndex(
          (item: FaceGazeCommandPair) => item.equals(newCommandPair));
      if (updateIndex < 0) {
        // Add new gesture/action pairing if it does not already exist.
        this.addNewCommandPair_(newCommandPair);
      }
    }

    this.faceGazeActionsAlert_ = this.getAlertText_(newCommandPair);
  }

  private getAlertText_(commandPair: FaceGazeCommandPair): string {
    let actionDisplayText =
        this.i18n(FaceGazeUtils.getMacroDisplayTextName(commandPair.action));
    if (commandPair.action === MacroName.CUSTOM_KEY_COMBINATION &&
        commandPair.assignedKeyCombo) {
      const keyComboDisplayText = this.getKeyComboDisplayText_(commandPair);
      actionDisplayText = keyComboDisplayText!;
    }

    return this.i18n(
        'faceGazeActionsAssignedGestureAlert',
        this.i18n(FaceGazeUtils.getGestureDisplayTextName(commandPair.gesture)),
        actionDisplayText);
  }

  private addCommandPairToPref_(newCommandPair: FaceGazeCommandPair): void {
    if (!newCommandPair.gesture) {
      console.error(
          'FaceGaze added action with no valid gesture value: ' +
          this.getActionDisplayText_(newCommandPair.action));
      return;
    }

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

        delete assignedGestures[currentGesture];
        break;
      }
    }

    if (!alreadySet) {
      assignedGestures[newCommandPair.gesture] = newCommandPair.action;
    }

    this.set(FACE_GAZE_GESTURE_TO_MACROS_PREF, assignedGestures);

    if (newCommandPair.action === MacroName.CUSTOM_KEY_COMBINATION) {
      this.addKeyComboToPref_(newCommandPair);
    }
  }

  private addKeyComboToPref_(newCommandPair: FaceGazeCommandPair): void {
    if (!newCommandPair.gesture) {
      throw new Error(
          `Expected gesture to be assigned to action ${newCommandPair.action}`);
    }

    if (!newCommandPair.assignedKeyCombo) {
      throw new Error(this.getKeyComboErrorMessage_(newCommandPair.gesture));
    }

    this.setPrefDictEntry(
        FACE_GAZE_GESTURE_TO_KEY_COMBO_PREF_DICT, newCommandPair.gesture,
        newCommandPair.assignedKeyCombo.prefString);
  }

  private removeKeyComboFromPref_(
      gesture: FacialGesture, removedKeyCombo?: AssignedKeyCombo): void {
    if (!removedKeyCombo) {
      throw new Error(this.getKeyComboErrorMessage_(gesture));
    }

    const assignedKeyCombos = this.getCurrentKeyCombos_();

    for (const [currentGesture, keyCombo] of Object.entries(
             assignedKeyCombos)) {
      if (currentGesture === gesture &&
          keyCombo === removedKeyCombo.prefString) {
        delete assignedKeyCombos[currentGesture as FacialGesture];
        break;
      }
    }

    this.set(FACE_GAZE_GESTURE_TO_KEY_COMBO_PREF, assignedKeyCombos);
  }

  // Initialize list of command pairs to display from the user prefs on load or
  // when the feature is turned on for the first time.
  private initFromPrefs_(): void {
    if (this.commandPairs_.length !== 0) {
      // Only initialize if loading for the first time.
      return;
    }
    const assignedGestures = this.getCurrentAssignedGestures_();
    const currentKeyCombos = this.getCurrentKeyCombos_();

    // Since the 'gesture to macro' pref and 'gesture to key combo' pref are
    // saved separately, there is a chance for the prefs to become malformed
    // if a 'gesture to key combo macro' mapping is saved to the 'gesture to
    // macro' pref without the corresponding 'gesture to key combo' pref. This
    // should only occur if a user was previously on a non-release build. If
    // this occurs, then remove the 'gesture to key combo macro' mapping
    // altogether from the pref to restore the user prefs to a valid state.
    let shouldFixPref = false;
    for (const [currentGesture, assignedMacro] of Object.entries(
             assignedGestures)) {
      if (assignedMacro !== MacroName.UNSPECIFIED) {
        const newGesture = currentGesture as FacialGesture;
        const newCommandPair =
            new FaceGazeCommandPair(assignedMacro, newGesture);

        if (assignedMacro === MacroName.CUSTOM_KEY_COMBINATION) {
          const keyCombo = currentKeyCombos[newGesture];

          // Log error instead of throwing to ensure the user can access the
          // settings, then correct the malformed pref.
          if (!keyCombo) {
            shouldFixPref = true;
            console.error(`${this.getKeyComboErrorMessage_(newGesture)}
                Deleting assignment to custom key combination action.`);
            delete assignedGestures[newGesture];
            continue;
          }

          newCommandPair.assignedKeyCombo = new AssignedKeyCombo(keyCombo);
        }

        this.addNewCommandPair_(newCommandPair);
      }
    }

    if (shouldFixPref) {
      this.set(FACE_GAZE_GESTURE_TO_MACROS_PREF, assignedGestures);
    }
  }

  private addNewCommandPair_(newCommandPair: FaceGazeCommandPair): void {
    // Ensure TOGGLE_FACEGAZE and RESET_CURSOR are at the top of the displayed
    // list.
    if (newCommandPair.action === MacroName.TOGGLE_FACEGAZE ||
        newCommandPair.action === MacroName.RESET_CURSOR) {
      this.unshift(
          FaceGazeActionsCardElement.FACEGAZE_COMMAND_PAIRS_PROPERTY_NAME,
          newCommandPair);
    } else {
      this.push(
          FaceGazeActionsCardElement.FACEGAZE_COMMAND_PAIRS_PROPERTY_NAME,
          newCommandPair);
    }
  }

  private updateCommandPairGesture_(
      index: number, newGesture: FacialGesture|null): void {
    const updateCommandPair = this.commandPairs_[index];
    const unassignGesture = updateCommandPair.gesture;

    // Update configuration and notify Polymer.
    this.commandPairs_[index].gesture = newGesture;
    this.notifyPath(
        `${FaceGazeActionsCardElement.FACEGAZE_COMMAND_PAIRS_PROPERTY_NAME}.${
            index}.gesture`);

    // If the command pair being updated contains a key combo and is being
    // updated to unassign a gesture, first unassign the gesture then unassign
    // the key combo so FaceGaze does not attempt to execute macro for gesture.
    if (updateCommandPair.action === MacroName.CUSTOM_KEY_COMBINATION &&
        unassignGesture && !newGesture) {
      this.removeKeyComboFromPref_(
          unassignGesture, this.commandPairs_[index].assignedKeyCombo);
    }
  }

  private computeLeftClickGestures_(): FacialGesture[] {
    const gestures: FacialGesture[] = [];
    this.commandPairs_.forEach((commandPair: FaceGazeCommandPair) => {
      if (commandPair.action === MacroName.MOUSE_CLICK_LEFT &&
          commandPair.gesture !== null) {
        gestures.push(commandPair.gesture);
      }
    });

    return gestures;
  }

  private getKeyComboErrorMessage_(gesture: FacialGesture|null): string {
    return `FaceGaze expected key combination to be assigned to ${gesture}.`;
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
