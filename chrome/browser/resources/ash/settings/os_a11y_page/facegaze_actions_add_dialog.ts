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

import {FacialGesture} from 'chrome://resources/ash/common/accessibility/facial_gestures.js';
import {MacroName} from 'chrome://resources/ash/common/accessibility/macro_names.js';
import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {CrScrollableMixin} from 'chrome://resources/ash/common/cr_elements/cr_scrollable_mixin.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './facegaze_actions_add_dialog.html.js';
import {FACEGAZE_COMMAND_PAIR_ADDED_EVENT_NAME, FaceGazeActions, FaceGazeCommandPair, FaceGazeGestures, FaceGazeUtils} from './facegaze_constants.js';

export interface FaceGazeAddActionDialogElement {
  $: {
    dialog: CrDialogElement,
  };
}

export enum AddDialogPage {
  SELECT_ACTION = 0,
  SELECT_GESTURE = 1,
  GESTURE_THRESHOLD = 2,
}

const FaceGazeAddActionDialogElementBase =
    I18nMixin(CrScrollableMixin(PolymerElement));

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
  private selectedAction_: MacroName|null = null;
  private selectedGesture_: FacialGesture|null = null;
  private currentPage_: AddDialogPage = AddDialogPage.SELECT_ACTION;

  // Computed properties.
  private displayedActions_: MacroName[] = FaceGazeActions;

  // TODO(b:353403651): If left-click action is assigned to a singular gesture
  // then remove it from the list of available gestures to avoid losing left
  // click functionality.
  private displayedGestures_: FacialGesture[] = FaceGazeGestures;

  private getItemClass_(selected: boolean): 'selected'|'' {
    return selected ? 'selected' : '';
  }

  private getLocalizedSelectGestureTitle_(): string {
    return this.i18n(
        'faceGazeActionsDialogSelectGestureTitle',
        this.selectedAction_ ?
            FaceGazeUtils.getMacroDisplayText(this.selectedAction_) :
            '');
  }

  private getActionDisplayText_(action: MacroName): string {
    return FaceGazeUtils.getMacroDisplayText(action);
  }

  private getGestureDisplayText_(gesture: FacialGesture|null): string {
    return FaceGazeUtils.getGestureDisplayText(gesture);
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
    if (this.selectedAction_ === null) {
      console.error(
          'FaceGaze Add Dialog clicked save button but no action has been selected. Closing dialog.');
      this.$.dialog.close();
      return;
    }

    const commandPair =
        new FaceGazeCommandPair(this.selectedAction_, this.selectedGesture_);
    const event = new CustomEvent(FACEGAZE_COMMAND_PAIR_ADDED_EVENT_NAME, {
      bubbles: true,
      composed: true,
      detail: commandPair,
    });
    this.dispatchEvent(event);

    this.$.dialog.close();
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

  interface HTMLElementEventMap {
    [FACEGAZE_COMMAND_PAIR_ADDED_EVENT_NAME]: CustomEvent<FaceGazeCommandPair>;
  }
}
