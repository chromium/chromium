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

import {CrSliderElement} from '//resources/ash/common/cr_elements/cr_slider/cr_slider.js';
import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {FacialGesture} from 'chrome://resources/ash/common/accessibility/facial_gestures.js';
import {MacroName} from 'chrome://resources/ash/common/accessibility/macro_names.js';
import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {CrScrollableMixin} from 'chrome://resources/ash/common/cr_elements/cr_scrollable_mixin.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './facegaze_actions_add_dialog.html.js';
import {FACE_GAZE_GESTURE_TO_CONFIDENCE_PREF, FACE_GAZE_GESTURE_TO_CONFIDENCE_PREF_DICT, FACEGAZE_COMMAND_PAIR_ADDED_EVENT_NAME, FaceGazeActions, FaceGazeCommandPair, FaceGazeGestures, FaceGazeUtils} from './facegaze_constants.js';
import {FaceGazeSubpageBrowserProxy, FaceGazeSubpageBrowserProxyImpl} from './facegaze_subpage_browser_proxy.js';

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

export const FACEGAZE_CONFIDENCE_DEFAULT = 60;
export const FACEGAZE_CONFIDENCE_MIN = 1;
export const FACEGAZE_CONFIDENCE_MAX = 100;
export const FACEGAZE_CONFIDENCE_BUTTON_STEP = 5;

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
      currentPage_: {
        type: Number,
        observer: 'currentPageChanged_',
      },

      initialPage: {
        type: Object,
        observer: 'initialPageChanged_',
      },

      actionToAssignGesture: {
        type: Object,
        observer: 'actionToAssignGestureChanged_',
      },

      gestureToConfigure: {
        type: Object,
        observer: 'gestureToConfigureChanged_',
      },

      leftClickGestures: {
        type: Array,
        value: () => [],
        observer: 'leftClickGesturesChanged_',
      },

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
        observer: 'updateGestureThresholdValueFromGesture_',
      },

      localizedGestureThresholdTitle_: {
        type: String,
        computed: 'getLocalizedGestureThresholdTitle_(selectedGesture_)',
      },

      gestureThresholdValue_: {
        type: Number,
      },

      disableActionNextButton_: {
        type: Boolean,
        computed: 'shouldDisableActionNextButton_(selectedAction_)',
      },

      disableGestureNextButton_: {
        type: Boolean,
        computed: 'shouldDisableGestureNextButton_(selectedGesture_)',
      },

      displayGesturePreviousButton_: {
        type: Boolean,
        computed: 'shouldDisplayGesturePreviousButton_(initialPage)',
      },

      displayThresholdPreviousButton_: {
        type: Boolean,
        computed: 'shouldDisplayThresholdPreviousButton_(initialPage)',
      },
    };
  }

  static get observers() {
    return ['updateGestureThresholdValueFromGesture_(selectedGesture_)'];
  }

  actionToAssignGesture: MacroName|null = null;
  initialPage: AddDialogPage = AddDialogPage.SELECT_ACTION;
  gestureToConfigure: FacialGesture|null = null;
  leftClickGestures: FacialGesture[] = [];

  // Internal state.
  private selectedAction_: MacroName|null = null;
  private selectedGesture_: FacialGesture|null = null;
  private gestureThresholdValue_: number;
  private currentPage_: AddDialogPage = AddDialogPage.SELECT_ACTION;

  // Computed properties.
  private displayedActions_: MacroName[] = FaceGazeActions;
  private displayedGestures_: FacialGesture[] = FaceGazeGestures;

  private faceGazeSubpageBrowserProxy_: FaceGazeSubpageBrowserProxy;

  constructor() {
    super();
    this.faceGazeSubpageBrowserProxy_ =
        FaceGazeSubpageBrowserProxyImpl.getInstance();
  }

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

  private getLocalizedGestureThresholdTitle_(): string {
    return this.i18n(
        'faceGazeActionsDialogGestureThresholdTitle',
        this.selectedGesture_ ?
            FaceGazeUtils.getGestureDisplayText(this.selectedGesture_) :
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

  private shouldDisplayGesturePreviousButton_(): boolean {
    // Only show the previous button on the gesture page if we are starting from
    // the beginning of the dialog flow.
    return this.initialPage === AddDialogPage.SELECT_ACTION;
  }

  private shouldDisplayThresholdPreviousButton_(): boolean {
    // Only show the previous button on the threshold page if we are starting
    // from an earlier dialog flow.
    return this.initialPage !== AddDialogPage.GESTURE_THRESHOLD;
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

  private currentPageChanged_(newPage: AddDialogPage, oldPage: AddDialogPage):
      void {
    // Only toggle request for gesture information on if we are on the gesture
    // threshold page, which requires information about detected gestures,
    // or toggle request off if we are switching away from the gesture
    // threshold page.
    if (newPage === AddDialogPage.GESTURE_THRESHOLD ||
        oldPage === AddDialogPage.GESTURE_THRESHOLD) {
      this.faceGazeSubpageBrowserProxy_.toggleGestureInfoForSettings(
          newPage === AddDialogPage.GESTURE_THRESHOLD);
    }
  }

  private initialPageChanged_(page: AddDialogPage): void {
    this.currentPage_ = page;
  }

  private actionToAssignGestureChanged_(newValue: MacroName|null): void {
    if (!newValue) {
      return;
    }

    this.selectedAction_ = newValue;
  }

  private gestureToConfigureChanged_(newValue: FacialGesture|null): void {
    if (!newValue) {
      return;
    }

    this.selectedGesture_ = newValue;
  }

  // If left-click action is assigned to a singular gesture then remove it
  // from the list of available gestures to avoid losing left click
  // functionality.
  private leftClickGesturesChanged_(leftClickGestures: FacialGesture[]): void {
    if (leftClickGestures.length === 1) {
      this.displayedGestures_ =
          this.displayedGestures_.filter((gesture: FacialGesture) => {
            return leftClickGestures[0] !== gesture;
          });
    }
  }

  // Button event handlers
  private onCancelButtonClick_(): void {
    this.close_();
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
    this.gestureThresholdValue_ = Math.max(
        FACEGAZE_CONFIDENCE_MIN,
        this.gestureThresholdValue_ - FACEGAZE_CONFIDENCE_BUTTON_STEP);
  }

  private onIncreaseThresholdButtonClick_(): void {
    this.gestureThresholdValue_ = Math.min(
        FACEGAZE_CONFIDENCE_MAX,
        this.gestureThresholdValue_ + FACEGAZE_CONFIDENCE_BUTTON_STEP);
  }

  private getThresholdSlider(): CrSliderElement {
    const slider = this.shadowRoot?.querySelector<CrSliderElement>(
        '#faceGazeGestureThresholdSlider');
    assert(slider);
    return slider;
  }

  private onThresholdSliderChanged_(): void {
    this.gestureThresholdValue_ = this.getThresholdSlider().value;
  }

  private updateGestureThresholdValueFromGesture_(): void {
    this.gestureThresholdValue_ = FACEGAZE_CONFIDENCE_DEFAULT;
    const gesturesToConfidence = this.get(FACE_GAZE_GESTURE_TO_CONFIDENCE_PREF);

    if (this.selectedGesture_ &&
        this.selectedGesture_ in gesturesToConfidence) {
      this.gestureThresholdValue_ = gesturesToConfidence[this.selectedGesture_];
    }
  }

  private onSaveButtonClick_(): void {
    if (this.selectedAction_ === null) {
      console.error(
          'FaceGaze Add Dialog clicked save button but no action has been selected. Closing dialog.');
      this.close_();
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

    this.setPrefDictEntry(
        FACE_GAZE_GESTURE_TO_CONFIDENCE_PREF_DICT, this.selectedGesture_,
        Math.round(this.getThresholdSlider().value));

    this.close_();
  }

  private close_(): void {
    this.faceGazeSubpageBrowserProxy_.toggleGestureInfoForSettings(false);
    this.$.dialog.close();
  }

  private onKeydown_(e: KeyboardEvent): void {
    // Close dialog if 'esc' is pressed.
    if (e.key === 'Escape') {
      this.close_();
    }
  }

  getCurrentPageForTest(): AddDialogPage {
    return this.currentPage_;
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
