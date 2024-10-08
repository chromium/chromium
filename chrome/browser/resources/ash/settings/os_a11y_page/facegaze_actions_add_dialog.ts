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
import 'chrome://resources/ash/common/shortcut_input_ui/shortcut_input.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import 'chrome://resources/polymer/v3_0/paper-ripple/paper-ripple.js';
import '../settings_shared.css.js';
import './facegaze_icons.html.js';

import {CrSliderElement} from '//resources/ash/common/cr_elements/cr_slider/cr_slider.js';
import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {FacialGesture} from 'chrome://resources/ash/common/accessibility/facial_gestures.js';
import {MacroName} from 'chrome://resources/ash/common/accessibility/macro_names.js';
import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {CrScrollableMixin} from 'chrome://resources/ash/common/cr_elements/cr_scrollable_mixin.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/ash/common/cr_elements/web_ui_listener_mixin.js';
import {ShortcutInputElement} from 'chrome://resources/ash/common/shortcut_input_ui/shortcut_input.js';
import {ModifierKeyCodes} from 'chrome://resources/ash/common/shortcut_input_ui/shortcut_utils.js';
import {assert} from 'chrome://resources/js/assert.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {KeyEvent, ShortcutInputProviderInterface} from '../device_page/input_device_settings_types.js';
import {getShortcutInputProvider} from '../device_page/shortcut_input_mojo_interface_provider.js';

import {getTemplate} from './facegaze_actions_add_dialog.html.js';
import {AssignedKeyCombo, FACE_GAZE_GESTURE_TO_CONFIDENCE_PREF, FACE_GAZE_GESTURE_TO_CONFIDENCE_PREF_DICT, FACEGAZE_COMMAND_PAIR_ADDED_EVENT_NAME, FaceGazeActions, FaceGazeCommandPair, FaceGazeGestures, FaceGazeLocationDependentActions, FaceGazeLookGestures, FaceGazeUtils, KeyCombination} from './facegaze_constants.js';
import {FaceGazeSubpageBrowserProxy, FaceGazeSubpageBrowserProxyImpl} from './facegaze_subpage_browser_proxy.js';

export interface FaceGazeAddActionDialogElement {
  $: {
    dialog: CrDialogElement,
    shortcutInput: ShortcutInputElement,
  };
}

export enum AddDialogPage {
  SELECT_ACTION = 0,
  SELECT_GESTURE = 1,
  GESTURE_THRESHOLD = 2,
  CUSTOM_KEYBOARD = 3,
}

export enum Navigation {
  PREVIOUS = 0,
  NEXT = 1,
}

export class PageNavigation {
  previous?: AddDialogPage;
  next?: AddDialogPage;
}

export class FaceGazeGestureConfidence {
  gesture: FacialGesture;
  confidence: number;
}

export const FACEGAZE_DEFINED_MACRO_FLOW:
    Record<AddDialogPage, PageNavigation> = {
      [AddDialogPage.SELECT_ACTION]: {
        next: AddDialogPage.SELECT_GESTURE,
      },
      [AddDialogPage.SELECT_GESTURE]: {
        previous: AddDialogPage.SELECT_ACTION,
        next: AddDialogPage.GESTURE_THRESHOLD,
      },
      [AddDialogPage.GESTURE_THRESHOLD]: {
        previous: AddDialogPage.SELECT_GESTURE,
      },
      [AddDialogPage.CUSTOM_KEYBOARD]: {},
    };

export const FACEGAZE_CUSTOM_KEYBOARD_SHORTCUT_FLOW:
    Record<AddDialogPage, PageNavigation> = {
      [AddDialogPage.SELECT_ACTION]: {
        next: AddDialogPage.CUSTOM_KEYBOARD,
      },
      [AddDialogPage.SELECT_GESTURE]: {
        previous: AddDialogPage.CUSTOM_KEYBOARD,
        next: AddDialogPage.GESTURE_THRESHOLD,
      },
      [AddDialogPage.GESTURE_THRESHOLD]: {
        previous: AddDialogPage.SELECT_GESTURE,
      },
      [AddDialogPage.CUSTOM_KEYBOARD]: {
        previous: AddDialogPage.SELECT_ACTION,
        next: AddDialogPage.SELECT_GESTURE,
      },
    };

export type ShortcutInputCompleteEvent = CustomEvent<{keyEvent: KeyEvent}>;

export const FACEGAZE_CONFIDENCE_DEFAULT = 60;
export const FACEGAZE_CONFIDENCE_MIN = 1;
export const FACEGAZE_CONFIDENCE_MAX = 100;
export const FACEGAZE_CONFIDENCE_BUTTON_STEP = 5;

const FaceGazeAddActionDialogElementBase = PrefsMixin(
    I18nMixin(CrScrollableMixin(WebUiListenerMixin(PolymerElement))));

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

      commandPairToConfigure: {
        type: Object,
        observer: 'commandPairToConfigureChanged_',
      },

      leftClickGestures: {
        type: Array,
        value: () => [],
      },

      showSelectAction_: {
        type: Boolean,
        computed: 'shouldShowSelectAction_(currentPage_)',
      },

      showCustomKeyboard_: {
        type: Boolean,
        computed: 'shouldShowCustomKeyboard_(currentPage_)',
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
        observer: 'onSelectedActionChanged_',
      },

      keyCombination_: {
        type: Object,
      },

      localizedSelectGestureTitle_: {
        type: String,
        computed: 'getLocalizedSelectGestureTitle_(selectedAction_)',
      },

      displayedGestures_: {
        type: Array,
        computed: 'computeAllowedGestures_(selectedAction_, leftClickGestures)',
      },

      selectedGesture_: {
        type: Object,
        value: null,
        observer: 'onSelectedGestureChanged_',
      },

      localizedGestureThresholdTitle_: {
        type: String,
        computed: 'getLocalizedGestureThresholdTitle_(selectedGesture_)',
      },

      localizedGestureCountLabel_: {
        type: String,
        computed: 'getLocalizedGestureCountLabel_(detectedGestureCount_)',
      },

      gestureThresholdValue_: {
        type: Number,
        observer: 'onGestureThresholdChanged_',
      },

      detectedGestureCount_: {
        type: Number,
      },

      disableActionNextButton_: {
        type: Boolean,
        computed: 'shouldDisableActionNextButton_(selectedAction_)',
      },

      disableCustomKeyboardNextButton_: {
        type: Boolean,
        computed: 'shouldDisableCustomKeyboardNextButton_(keyCombination_)',
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

  initialPage: AddDialogPage = AddDialogPage.SELECT_ACTION;
  commandPairToConfigure: FaceGazeCommandPair|null = null;
  leftClickGestures: FacialGesture[] = [];
  shortcutInput: ShortcutInputElement|null;

  // Internal state.
  private selectedAction_: MacroName;
  private keyCombination_: KeyCombination|null = null;
  private selectedGesture_: FacialGesture|null = null;
  private gestureThresholdValue_: number;
  private currentPage_: AddDialogPage = AddDialogPage.SELECT_ACTION;
  private pageNavigation_: Record<AddDialogPage, PageNavigation> =
      FACEGAZE_DEFINED_MACRO_FLOW;
  private detectedGestureCount_ = 0;
  private eventTracker_: EventTracker = new EventTracker();
  private stream_: MediaStream|null;
  private streamTrack_: MediaStreamTrack|null;

  private displayedActions_: MacroName[] = FaceGazeActions;

  private faceGazeSubpageBrowserProxy_: FaceGazeSubpageBrowserProxy;

  constructor() {
    super();
    this.faceGazeSubpageBrowserProxy_ =
        FaceGazeSubpageBrowserProxyImpl.getInstance();
    this.addWebUiListener(
        'settings.sendGestureInfoToSettings',
        (gestureConfidences: FaceGazeGestureConfidence[]) =>
            this.onGestureConfidencesReceived_(gestureConfidences));
    this.eventTracker_.add(
        this, 'shortcut-input-event', this.onShortcutInputEvent_);
  }

  protected onShortcutInputDomChange(): void {
    // Start observing for input events the moment `shortcutInput` is available.
    this.shortcutInput =
        this.shadowRoot!.querySelector<ShortcutInputElement>('#shortcutInput');
    if (this.shortcutInput) {
      this.shortcutInput.startObserving();
    }
  }

  private getItemClass_(selected: boolean): 'selected'|'' {
    return selected ? 'selected' : '';
  }

  private getShortcutInputProvider(): ShortcutInputProviderInterface {
    return getShortcutInputProvider();
  }

  private onShortcutInputEvent_(e: ShortcutInputCompleteEvent): void {
    this.keyCombination_ = this.formatKeyCombination_(e.detail.keyEvent);
  }

  private formatKeyCombination_(keyEvent: KeyEvent): KeyCombination|null {
    if (!this.shortcutInput) {
      return null;
    }

    // Do not support a key combination consisting of only a modifier key.
    if (ModifierKeyCodes.includes(keyEvent.vkey as number)) {
      return null;
    }

    const newKeyCombination:
        KeyCombination = {key: keyEvent.vkey, keyDisplay: keyEvent.keyDisplay};

    const modifiers = this.shortcutInput.getModifiers(keyEvent);
    if (modifiers.length > 0) {
      newKeyCombination.modifiers = {};
      modifiers.forEach((modifier: string) => {
        switch (modifier) {
          case 'ctrl':
            newKeyCombination.modifiers!.ctrl = true;
            break;
          case 'alt':
            newKeyCombination.modifiers!.alt = true;
            break;
          case 'shift':
            newKeyCombination.modifiers!.shift = true;
            break;
          case 'meta':
            // TODO(b:366052411): Investigate support for meta keys other than
            // search.
            newKeyCombination.modifiers!.search = true;
            break;
        }
      });
    }

    return newKeyCombination;
  }

  private getLocalizedSelectGestureTitle_(): string {
    return this.i18n(
        'faceGazeActionsDialogSelectGestureTitle',
        this.selectedAction_ ? this.i18n(FaceGazeUtils.getMacroDisplayTextName(
                                   this.selectedAction_)) :
                               '');
  }

  private getLocalizedGestureThresholdTitle_(): string {
    return this.i18n(
        'faceGazeActionsDialogGestureThresholdTitle',
        this.selectedGesture_ ?
            this.i18n(FaceGazeUtils.getGestureDisplayTextName(
                this.selectedGesture_)) :
            '');
  }

  private getLocalizedGestureCountLabel_(): string {
    if (this.detectedGestureCount_ === 0) {
      return this.i18n('faceGazeActionsDialogGestureNotDetectedLabel');
    } else if (this.detectedGestureCount_ === 1) {
      return this.i18n('faceGazeActionsDialogGestureDetectedCountOneLabel');
    } else {
      return this.i18n(
          'faceGazeActionsDialogGestureDetectedCountLabel',
          this.detectedGestureCount_);
    }
  }

  private getActionDisplayText_(action: MacroName): string {
    return this.i18n(FaceGazeUtils.getMacroDisplayTextName(action));
  }

  private getGestureDisplayText_(gesture: FacialGesture|null): string {
    return this.i18n(FaceGazeUtils.getGestureDisplayTextName(gesture));
  }

  private getGestureIconName_(gesture: FacialGesture|null): string {
    return `facegaze:${FaceGazeUtils.getGestureIconName(gesture)}`;
  }

  // Dialog page navigation.
  private shouldShowSelectAction_(): boolean {
    return this.currentPage_ === AddDialogPage.SELECT_ACTION;
  }

  private shouldShowCustomKeyboard_(): boolean {
    return this.currentPage_ === AddDialogPage.CUSTOM_KEYBOARD;
  }

  private shouldShowSelectGesture_(): boolean {
    return this.currentPage_ === AddDialogPage.SELECT_GESTURE;
  }

  private shouldShowGestureThreshold_(): boolean {
    return this.currentPage_ === AddDialogPage.GESTURE_THRESHOLD;
  }

  // Disable navigation buttons.
  private shouldDisableActionNextButton_(): boolean {
    return this.selectedAction_ === null;
  }

  private shouldDisableCustomKeyboardNextButton_(): boolean {
    return this.keyCombination_ === null;
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

  // Dialog navigation button event handlers.
  private onPreviousButtonClick_(): void {
    this.onNavigateButtonClick_(Navigation.PREVIOUS);
  }

  private onNextButtonClick_(): void {
    this.onNavigateButtonClick_(Navigation.NEXT);
  }

  private onNavigateButtonClick_(direction: Navigation): void {
    const newPage = direction === Navigation.PREVIOUS ?
        this.pageNavigation_[this.currentPage_].previous :
        this.pageNavigation_[this.currentPage_].next;

    if (newPage === undefined) {
      // This should never happen.
      throw new Error(`Tried to navigate in ${
          direction.toString()} direction from page index ${
          this.currentPage_.toString()}`);
    }

    this.currentPage_ = newPage;
  }

  private onCancelButtonClick_(): void {
    this.close_();
  }

  private onSaveButtonClick_(): void {
    if (!this.selectedAction_ || !this.selectedGesture_) {
      console.error(
          'FaceGaze Add Dialog clicked save button but no action/gesture pair selected. Closing dialog.');
      this.close_();
      return;
    }

    const commandPair =
        new FaceGazeCommandPair(this.selectedAction_, this.selectedGesture_);

    if (this.selectedAction_ === MacroName.CUSTOM_KEY_COMBINATION) {
      if (!this.keyCombination_) {
        throw new Error(
            'FaceGaze selected custom key combination action but no key combination set.');
      }

      commandPair.assignedKeyCombo =
          new AssignedKeyCombo(JSON.stringify(this.keyCombination_));
    }

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

    if (oldPage === AddDialogPage.CUSTOM_KEYBOARD && this.shortcutInput) {
      this.shortcutInput.stopObserving();
    }
  }

  // Handlers for initial state.
  private initialPageChanged_(page: AddDialogPage): void {
    this.currentPage_ = page;
  }

  private commandPairToConfigureChanged_(newValue: FaceGazeCommandPair|
                                         null): void {
    if (!newValue) {
      return;
    }

    this.selectedAction_ = newValue.action;
    this.selectedGesture_ = newValue.gesture;
    if (newValue.assignedKeyCombo) {
      this.keyCombination_ = newValue.assignedKeyCombo.keyCombo;
    }
  }

  private computeAllowedGestures_(): FacialGesture[] {
    let displayedGestures: FacialGesture[] = FaceGazeGestures;
    // If left-click action is assigned to a singular gesture then remove it
    // from the list of available gestures to avoid losing left click
    // functionality.
    if (this.leftClickGestures.length === 1) {
      displayedGestures = displayedGestures.filter((gesture: FacialGesture) => {
        return this.leftClickGestures[0] !== gesture;
      });
    }

    // If the selected action is dependent on location, then only allow gestures
    // where the user can be looking at their screen while performing it so they
    // can be certain of their mouse location.
    if (FaceGazeLocationDependentActions.includes(this.selectedAction_)) {
      displayedGestures = displayedGestures.filter((gesture: FacialGesture) => {
        return !FaceGazeLookGestures.includes(gesture);
      });
    }

    return displayedGestures;
  }

  private async onThresholdPageDomChanged_(): Promise<void> {
    if (this.currentPage_ !== AddDialogPage.GESTURE_THRESHOLD) {
      return;
    }

    const videoElement =
        this.shadowRoot!.querySelector<HTMLMediaElement>('#cameraStream');

    if (videoElement) {
      this.stream_ = await navigator.mediaDevices.getUserMedia({
        audio: false,
        video: {facingMode: 'user'},
      });
      this.streamTrack_ = this.stream_.getVideoTracks()[0];
      videoElement!.srcObject = this.stream_;
    }
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

  private onGestureConfidencesReceived_(info: FaceGazeGestureConfidence[]):
      void {
    if (!this.selectedGesture_ ||
        this.currentPage_ !== AddDialogPage.GESTURE_THRESHOLD) {
      return;
    }

    info.forEach((entry) => {
      if (entry.gesture === this.selectedGesture_) {
        const previewContainer = this.shadowRoot!.querySelector<HTMLElement>(
            '#cameraPreviewContainer');
        assert(previewContainer);

        if (entry.confidence >= this.gestureThresholdValue_) {
          this.detectedGestureCount_++;
          previewContainer.className = 'gesture-detected';
        } else {
          previewContainer.className = 'gesture-not-detected';
        }

        // Show confidence values for all gestures in dynamic bar.
        const slider = this.getThresholdSlider();
        const sliderBar = slider.shadowRoot!.querySelector<HTMLElement>('#bar');
        assert(sliderBar);
        sliderBar.style.width = `${entry.confidence}%`;
      }
    });
  }

  private onSelectedActionChanged_(): void {
    if (this.selectedAction_ === MacroName.CUSTOM_KEY_COMBINATION) {
      this.pageNavigation_ = FACEGAZE_CUSTOM_KEYBOARD_SHORTCUT_FLOW;
    } else {
      this.pageNavigation_ = FACEGAZE_DEFINED_MACRO_FLOW;
    }
  }

  private onSelectedGestureChanged_(): void {
    this.detectedGestureCount_ = 0;
    this.gestureThresholdValue_ = FACEGAZE_CONFIDENCE_DEFAULT;
    const gesturesToConfidence = this.get(FACE_GAZE_GESTURE_TO_CONFIDENCE_PREF);

    if (this.selectedGesture_ &&
        this.selectedGesture_ in gesturesToConfidence) {
      this.gestureThresholdValue_ = gesturesToConfidence[this.selectedGesture_];
    }
  }

  private onGestureThresholdChanged_(): void {
    this.detectedGestureCount_ = 0;
  }

  private close_(): void {
    if (this.shortcutInput) {
      this.shortcutInput.stopObserving();
    }

    if (this.streamTrack_) {
      this.streamTrack_.stop();
    }

    if (this.stream_) {
      this.stream_ = null;
    }

    this.commandPairToConfigure = null;
    this.keyCombination_ = null;
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
