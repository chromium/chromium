// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'facegaze-actions-add-dialog' is a dialog for
 * adding an action to FaceGaze.
 */
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/ash/common/cr_elements/cr_slider/cr_slider.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import 'chrome://resources/polymer/v3_0/paper-ripple/paper-ripple.js';
import '../settings_shared.css.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {FacialGesture} from 'chrome://resources/ash/common/accessibility/facial_gestures.js';
import {MacroName} from 'chrome://resources/ash/common/accessibility/macro_names.js';
import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {CrScrollableMixin} from 'chrome://resources/ash/common/cr_elements/cr_scrollable_mixin.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './facegaze_actions_add_dialog.html.js';

export interface FaceGazeAddActionDialogElement {
  $: {
    dialog: CrDialogElement,
  };
}

export interface ActionItem {
  value: MacroName;
  displayText: string;
}

export interface GestureItem {
  value: FacialGesture;
  displayText: string;
}

export enum AddDialogPage {
  SELECT_ACTION = 0,
  SELECT_GESTURE = 1,
  GESTURE_THRESHOLD = 2,
}

const FaceGazeAddActionDialogElementBase =
    PrefsMixin(I18nMixin(CrScrollableMixin(PolymerElement)));

export class FaceGazeAddActionDialogElement extends
    FaceGazeAddActionDialogElementBase {
  static get is() {
    return 'facegaze-actions-add-dialog' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      showSelectAction_: {
        type: Boolean,
        computed: 'shouldShowSelectAction_(currentPage_)',
      },

      showSelectGesture_: {
        type: Boolean,
        computed: 'shouldShowSelectGesture_(currentPage_)',
      },

      showGestureThreshold_: {
        type: Boolean,
        computed: 'shouldShowGestureThreshold_(currentPage_)',
      },

      displayedActions_: {
        type: Array,
        value: () => [],
      },

      selectedAction_: {
        type: Object,
        value: null,
      },

      localizedSelectGestureTitle_: {
        type: String,
        computed: 'getLocalizedSelectGestureTitle_(selectedAction_)',
      },

      displayedGestures_: {
        type: Array,
        value: () => [],
      },

      selectedGesture_: {
        type: Object,
        value: null,
      },

      disableActionNextButton_: {
        type: Boolean,
        computed: 'shouldDisableActionNextButton_(selectedAction_)',
      },

      disableGestureNextButton_: {
        type: Boolean,
        computed: 'shouldDisableGestureNextButton_(selectedGesture_)',
      },
    };
  }

  // Internal state.
  private selectedAction_: ActionItem;
  private selectedGesture_: GestureItem;
  private currentPage_: AddDialogPage = AddDialogPage.SELECT_ACTION;

  // Computed properties.
  private displayedActions_: ActionItem[];
  private displayedGestures_: GestureItem[];

  override ready(): void {
    super.ready();

    // TODO(b:341770753): Localize these strings.
    this.displayedActions_ = [
      {
        value: MacroName.MOUSE_CLICK_LEFT,
        displayText: 'Click a mouse button',
      },
      {
        value: MacroName.MOUSE_CLICK_RIGHT,
        displayText: 'Right-click the mouse',
      },
      {
        value: MacroName.MOUSE_LONG_CLICK_LEFT,
        displayText: 'Long click mouse',
      },
      {
        value: MacroName.RESET_CURSOR,
        displayText: 'Reset cursor to center',
      },
      {
        value: MacroName.TOGGLE_DICTATION,
        displayText: 'Start or stop dictation',
      },
      {
        value: MacroName.KEY_PRESS_SPACE,
        displayText: 'Press space key',
      },
      {
        value: MacroName.KEY_PRESS_DOWN,
        displayText: 'Press down key',
      },
      {
        value: MacroName.KEY_PRESS_LEFT,
        displayText: 'Press left key',
      },
      {
        value: MacroName.KEY_PRESS_RIGHT,
        displayText: 'Press right key',
      },
      {
        value: MacroName.KEY_PRESS_UP,
        displayText: 'Press up key',
      },
      {
        value: MacroName.KEY_PRESS_TOGGLE_OVERVIEW,
        displayText: 'Toggle overview',
      },
      {
        value: MacroName.KEY_PRESS_MEDIA_PLAY_PAUSE,
        displayText: 'Play or pause media',
      },
    ];

    this.displayedGestures_ = [
      {
        value: FacialGesture.BROW_INNER_UP,
        displayText: 'Brow inner up',
      },
      {
        value: FacialGesture.BROWS_DOWN,
        displayText: 'Brows down',
      },
      {
        value: FacialGesture.EYE_SQUINT_LEFT,
        displayText: 'Squint left eye',
      },
      {
        value: FacialGesture.EYE_SQUINT_RIGHT,
        displayText: 'Squint right eye',
      },
      {
        value: FacialGesture.EYES_BLINK,
        displayText: 'Eyes blink',
      },
      {
        value: FacialGesture.EYES_LOOK_DOWN,
        displayText: 'Eyes look down',
      },
      {
        value: FacialGesture.EYES_LOOK_LEFT,
        displayText: 'Eyes look left',
      },
      {
        value: FacialGesture.EYES_LOOK_RIGHT,
        displayText: 'Eyes look right',
      },
      {
        value: FacialGesture.EYES_LOOK_UP,
        displayText: 'Eyes look up',
      },
      {
        value: FacialGesture.JAW_OPEN,
        displayText: 'Jaw open',
      },
      {
        value: FacialGesture.MOUTH_LEFT,
        displayText: 'Mouth left',
      },
      {
        value: FacialGesture.MOUTH_PUCKER,
        displayText: 'Mouth pucker',
      },
      {
        value: FacialGesture.MOUTH_RIGHT,
        displayText: 'Mouth right',
      },
      {
        value: FacialGesture.MOUTH_SMILE,
        displayText: 'Mouth smile',
      },
      {
        value: FacialGesture.MOUTH_UPPER_UP,
        displayText: 'Mouth upper up',
      },
    ];
  }

  private getItemClass_(selected: boolean): 'selected'|'' {
    return selected ? 'selected' : '';
  }

  private getLocalizedSelectGestureTitle_(): string {
    return this.i18n(
        'faceGazeActionsDialogSelectGestureTitle',
        this.selectedAction_ ? this.selectedAction_.displayText : '');
  }

  private getDisplayText_(action: GestureItem): string {
    return action.displayText;
  }

  private getGestureDisplayText_(gesture: ActionItem): string {
    return gesture.displayText;
  }

  // Disable next buttons
  private shouldDisableActionNextButton_(): boolean {
    return this.selectedAction_ === null;
  }

  private shouldDisableGestureNextButton_(): boolean {
    return this.selectedGesture_ === null;
  }

  // Dialog page navigation
  private shouldShowSelectAction_(): boolean {
    return this.currentPage_ === AddDialogPage.SELECT_ACTION;
  }

  private shouldShowSelectGesture_(): boolean {
    return this.currentPage_ === AddDialogPage.SELECT_GESTURE;
  }

  private shouldShowGestureThreshold_(): boolean {
    return this.currentPage_ === AddDialogPage.GESTURE_THRESHOLD;
  }

  // Button event handlers
  private onCancelButtonClick_(): void {
    this.$.dialog.close();
  }

  private onActionNextButtonClick_(): void {
    this.currentPage_ = AddDialogPage.SELECT_GESTURE;
  }

  private onGesturePreviousButtonClick_(): void {
    this.currentPage_ = AddDialogPage.SELECT_ACTION;
  }

  private onGestureNextButtonClick_(): void {
    this.currentPage_ = AddDialogPage.GESTURE_THRESHOLD;
  }

  private onGestureThresholdPreviousButtonClick_(): void {
    this.currentPage_ = AddDialogPage.SELECT_GESTURE;
  }

  private onDecreaseThresholdButtonClick_(): void {
    // TODO(b:341770753): Implement button with slider. See
    // switch_access_setup_guide_dialog.ts onAutoScanSpeedSlower_ for example of
    // slider with increment/decrement buttons. Button interacts with pref
    // directly.
  }

  private onIncreaseThresholdButtonClick_(): void {
    // TODO(b:341770753): Implement button with slider. See
    // switch_access_setup_guide_dialog.ts onAutoScanSpeedSlower_ for example of
    // slider with increment/decrement buttons. Button interacts with pref
    // directly.
  }

  private onSaveButtonClick_(): void {
    // TODO(b:341770753): Save individual pref for this action/gesture.
  }

  private onKeydown_(e: KeyboardEvent): void {
    // Close dialog if 'esc' is pressed.
    if (e.key === 'Escape') {
      this.$.dialog.close();
    }
  }
}

customElements.define(
    FaceGazeAddActionDialogElement.is, FaceGazeAddActionDialogElement);

declare global {
  interface HTMLElementTagNameMap {
    [FaceGazeAddActionDialogElement.is]: FaceGazeAddActionDialogElement;
  }
}
