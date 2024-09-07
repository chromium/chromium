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
import {FacialGesture} from 'chrome://resources/ash/common/accessibility/facial_gestures.js';
import {MacroName} from 'chrome://resources/ash/common/accessibility/macro_names.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/ash/common/cr_elements/web_ui_listener_mixin.js';
import {DomRepeatEvent, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DeepLinkingMixin} from '../common/deep_linking_mixin.js';
import {RouteObserverMixin} from '../common/route_observer_mixin.js';
import {Route, routes} from '../router.js';

import {AddDialogPage} from './facegaze_actions_add_dialog.js';
import {getTemplate} from './facegaze_actions_card.html.js';
import {FACE_GAZE_GESTURE_TO_MACROS_PREF, FACEGAZE_ACTION_ASSIGN_GESTURE_EVENT_NAME, FACEGAZE_COMMAND_PAIR_ADDED_EVENT_NAME, FaceGazeCommandPair, FaceGazeUtils} from './facegaze_constants.js';

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
  private actionToAssignGesture_: MacroName|null = null;
  private gestureToConfigure_: FacialGesture|null = null;

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

      actionToAssignGesture_: {
        type: Object,
      },

      gestureToConfigure_: {
        type: Object,
      },
    };
  }

  override ready(): void {
    super.ready();
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

  private shouldDisableConfigureControls_(): boolean {
    return this.disabled ||
        !this.getPref('settings.a11y.face_gaze.actions_enabled').value;
  }

  private onAddActionButtonClick_(): void {
    this.dialogPageToShow_ = AddDialogPage.SELECT_ACTION;
    this.leftClickGestures_ = this.computeLeftClickGestures_();
    this.showAddActionDialog_ = true;
  }

  private onAddActionDialogClose_(): void {
    this.showAddActionDialog_ = false;
    this.actionToAssignGesture_ = null;
    this.gestureToConfigure_ = null;
  }

  private onConfigureGestureButtonClick_(
      e: DomRepeatEvent<FaceGazeCommandPair>): void {
    this.dialogPageToShow_ = AddDialogPage.GESTURE_THRESHOLD;
    this.gestureToConfigure_ = e.model.item.gesture;
    this.showAddActionDialog_ = true;
  }

  private onAssignGestureButtonClick_(e: DomRepeatEvent<FaceGazeCommandPair>):
      void {
    this.dialogPageToShow_ = AddDialogPage.SELECT_GESTURE;
    this.leftClickGestures_ = this.computeLeftClickGestures_();
    this.actionToAssignGesture_ = e.model.item.action;
    this.showAddActionDialog_ = true;
  }

  private getActionDisplayText_(action: MacroName): string {
    return FaceGazeUtils.getMacroDisplayText(action);
  }

  private getSubLabel_(action: MacroName): string|null {
    return FaceGazeUtils.getMacroDisplaySubLabel(action);
  }

  private getGestureDisplayText_(gesture: FacialGesture|null): string {
    return FaceGazeUtils.getGestureDisplayText(gesture);
  }

  private getGestureIconName_(gesture: FacialGesture|null): string {
    return `facegaze:${FaceGazeUtils.getGestureIconName(gesture)}`;
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

  // When an action is added from the dialog, update the pref and then update
  // the UI accordingly.
  private onCommandPairAdded_(
      e: HTMLElementEventMap[typeof FACEGAZE_COMMAND_PAIR_ADDED_EVENT_NAME]):
      void {
    const newCommandPair = e.detail;
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

    if (this.dialogPageToShow_ === AddDialogPage.SELECT_GESTURE) {
      // Update an existing row for the action if coming from the Assign a
      // Gesture page.
      const updateIndex = this.commandPairs_.findIndex(
          (item: FaceGazeCommandPair) =>
              item.action === newCommandPair.action && item.gesture === null);
      if (updateIndex > -1) {
        // Update configuration and notify Polymer.
        this.commandPairs_[updateIndex].gesture = newCommandPair.gesture;
        this.notifyPath(`${
            FaceGazeActionsCardElement.FACEGAZE_COMMAND_PAIRS_PROPERTY_NAME}.${
            updateIndex}.gesture`);
      }
    } else {
      const updateIndex = this.commandPairs_.findIndex(
          (item: FaceGazeCommandPair) => item.equals(newCommandPair));
      if (updateIndex < 0) {
        // Add new gesture/action pairing if it does not already exist.
        this.push(
            FaceGazeActionsCardElement.FACEGAZE_COMMAND_PAIRS_PROPERTY_NAME,
            newCommandPair);
      }
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
