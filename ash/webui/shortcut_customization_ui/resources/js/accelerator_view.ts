// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './input_key.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert_ts.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {mojoString16ToString} from 'chrome://resources/js/mojo_type_util.js';
import {String16} from 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AcceleratorResultData, UserAction} from '../mojom-webui/ash/webui/shortcut_customization_ui/mojom/shortcut_customization.mojom-webui.js';

import {AcceleratorLookupManager} from './accelerator_lookup_manager.js';
import {getTemplate} from './accelerator_view.html.js';
import {getShortcutProvider} from './mojo_interface_provider.js';
import {ModifierKeyCodes} from './shortcut_input.js';
import {Accelerator, AcceleratorConfigResult, AcceleratorKeyState, AcceleratorSource, AcceleratorState, Modifier, ShortcutProviderInterface, StandardAcceleratorInfo} from './shortcut_types.js';
import {createEmptyAcceleratorInfo, getAccelerator, getKeyDisplay, getModifiersForAcceleratorInfo, isCustomizationAllowed, isFunctionKey, isStandardAcceleratorInfo, keyCodeToModifier, keyToIconNameMap, LWIN_KEY, META_KEY, unidentifiedKeyCodeToKey} from './shortcut_utils.js';
export interface AcceleratorViewElement {
  $: {
    container: HTMLDivElement,
  };
}

enum KeyState {
  NOT_SELECTED = 'not-selected',
  MODIFIER = 'modifier-selected',
  ALPHANUMERIC = 'alpha-numeric-selected',
}

export enum ViewState {
  VIEW,
  ADD,
  EDIT,
}

// This delay should match the animation timing in `input_key.html`. Matching
// the delay allows the user to see the full animation before requesting a
// change to the backend.
const kAnimationTimeoutMs: number = 300;

const kEscapeKey: number = 27;  // Keycode for VKEY_ESCAPE

/**
 * @fileoverview
 * 'accelerator-view' is wrapper component for an accelerator. It maintains both
 * the read-only and editable state of an accelerator.
 * TODO(jimmyxgong): Implement the edit mode.
 */
const AcceleratorViewElementBase = I18nMixin(PolymerElement);

export class AcceleratorViewElement extends AcceleratorViewElementBase {
  static get is(): string {
    return 'accelerator-view';
  }

  static get properties(): PolymerElementProperties {
    return {
      acceleratorInfo: {
        type: Object,
      },

      pendingAcceleratorInfo: {
        type: Object,
      },

      viewState: {
        type: Number,
        value: ViewState.VIEW,
        notify: true,
        observer: AcceleratorViewElement.prototype.onViewStateChanged,
      },

      modifiers: {
        type: Array,
        computed: 'getModifiers(acceleratorInfo.accelerator.*)',
      },

      isCapturing: {
        type: Boolean,
        value: false,
      },

      statusMessage: {
        type: String,
        notify: true,
      },

      /** Informs parent components that an error has occurred. */
      hasError: {
        type: Boolean,
        value: false,
        notify: true,
      },

      action: {
        type: Number,
        value: 0,
      },

      source: {
        type: Number,
        value: 0,
      },

      sourceIsLocked: {
        type: Boolean,
        value: false,
      },

      /**
       * Conditionally show the edit-icon-container in `accelerator-view`, true
       * for `accelerator-row`, false for `accelerator-edit-view`.
       */
      showEditIcon: {
        type: Boolean,
        value: false,
      },

      /** Only show the edit button in the first row. */
      isFirstAccelerator: {
        type: Boolean,
      },

      isDisabled: {
        type: Boolean,
        computed: 'computeIsDisabled(acceleratorInfo.*)',
        reflectToAttribute: true,
      },

      highlighted: {
        type: Boolean,
        reflectToAttribute: true,
      },
    };
  }

  acceleratorInfo: StandardAcceleratorInfo;
  viewState: ViewState;
  statusMessage: string;
  hasError: boolean;
  action: number;
  source: AcceleratorSource;
  sourceIsLocked: boolean;
  showEditIcon: boolean;
  categoryIsLocked: boolean;
  isFirstAccelerator: boolean;
  isDisabled: boolean;
  highlighted: boolean;
  protected pendingAcceleratorInfo: StandardAcceleratorInfo;
  protected isCapturing: boolean;
  private modifiers: string[];
  private shortcutProvider: ShortcutProviderInterface = getShortcutProvider();
  private lookupManager: AcceleratorLookupManager =
      AcceleratorLookupManager.getInstance();
  private eventTracker: EventTracker = new EventTracker();

  override connectedCallback(): void {
    super.connectedCallback();

    this.categoryIsLocked = this.lookupManager.isCategoryLocked(
        this.lookupManager.getAcceleratorCategory(this.source, this.action));
  }

  override disconnectedCallback(): void {
    super.disconnectedCallback();
    this.eventTracker.removeAll();
  }

  private getModifiers(): string[] {
    return getModifiersForAcceleratorInfo(this.acceleratorInfo);
  }

  protected onViewStateChanged(): void {
    if (this.viewState !== ViewState.VIEW) {
      this.registerKeyEventListeners();
      return;
    }
    this.unregisterKeyEventListeners();
  }

  private registerKeyEventListeners(): void {
    this.eventTracker.add(
        this, 'keydown', (e: KeyboardEvent) => this.onKeyDown(e));
    this.eventTracker.add(this, 'keyup', (e: KeyboardEvent) => this.onKeyUp(e));
    this.eventTracker.add(this, 'focus', () => this.startCapture());
    this.eventTracker.add(this, 'mouseup', () => this.startCapture());
    this.eventTracker.add(
        this, 'blur', () => this.endCapture(/*should_delay=*/ false));
    this.$.container.focus();
  }

  private unregisterKeyEventListeners(): void {
    this.eventTracker.removeAll();
  }


  private async startCapture(): Promise<void> {
    if (this.isCapturing) {
      return;
    }
    // Disable ChromeOS accelerator handler when starting input capture.
    this.pendingAcceleratorInfo = createEmptyAcceleratorInfo();
    this.isCapturing = true;

    this.dispatchEvent(new CustomEvent('accelerator-capturing-started', {
      bubbles: true,
      composed: true,
    }));

    // Block processing accelerators.
    await this.shortcutProvider.preventProcessingAccelerators(true);
  }

  private async endCapture(shouldDelay: boolean): Promise<void> {
    if (!this.isCapturing) {
      return;
    }
    await this.shortcutProvider.preventProcessingAccelerators(false);

    this.isCapturing = false;
    this.dispatchEvent(new CustomEvent('accelerator-capturing-ended', {
      bubbles: true,
      composed: true,
    }));

    // Delay if an update event is fired.
    if (shouldDelay) {
      await new Promise(resolve => setTimeout(resolve, kAnimationTimeoutMs));
    }

    this.viewState = ViewState.VIEW;
    // Should always set `hasError` before `statusMessage` since `statusMessage`
    // is dependent on `hasError`'s state.
    this.hasError = false;
    this.statusMessage = '';
    this.pendingAcceleratorInfo = createEmptyAcceleratorInfo();
  }

  private onKeyDown(e: KeyboardEvent): void {
    if (!this.isCapturing) {
      return;
    }
    e.preventDefault();
    e.stopPropagation();
    this.handleKeyDown(e);
  }

  private onKeyUp(e: KeyboardEvent): void {
    if (!this.isCapturing || this.hasError) {
      return;
    }
    e.preventDefault();
    e.stopPropagation();
    this.handleKeyUp(e);
  }

  private handleKeyDown(e: KeyboardEvent): void {
    if (this.hasError) {
      // Reset status state when pressing the a new key.
      this.statusMessage = '';
      this.hasError = false;
    }

    const pendingAccelerator = this.keystrokeToAccelerator(e);
    // Alt + Esc will exit input handling immediately.
    if (pendingAccelerator.modifiers === Modifier.ALT &&
        pendingAccelerator.keyCode === kEscapeKey) {
      this.endCapture(/*shouldDelay=*/ false);
      return;
    }

    // Add the key pressed to pendingAccelerator.
    this.set(
        'pendingAcceleratorInfo.layoutProperties.standardAccelerator.accelerator',
        pendingAccelerator);

    if (this.isModifierKey(e)) {
      // Reset the keyDisplay property if the key is a modifier.
      this.set(
          'pendingAcceleratorInfo.layoutProperties.standardAccelerator.keyDisplay',
          '');
    } else {
      // Set keyDisplay property.
      this.set(
          'pendingAcceleratorInfo.layoutProperties.standardAccelerator.keyDisplay',
          this.getKeyDisplay(e));
    }

    // Only process valid accelerators.
    if (this.isValidDefaultAccelerator(this.pendingAcceleratorInfo)) {
      this.processPendingAccelerator(this.pendingAcceleratorInfo);
    }
  }

  private handleKeyUp(e: KeyboardEvent): void {
    const pendingAccelerator = this.pendingAcceleratorInfo.layoutProperties
                                   .standardAccelerator.accelerator;
    // Remove the modifier that was just released.
    if (this.isModifierKey(e)) {
      const modifier = keyCodeToModifier[e.keyCode];
      const pendingModifiers = pendingAccelerator.modifiers;
      // Assert that the released modifier is present in the pending
      // accelerator.
      assert(pendingModifiers & modifier);
      // Remove the released modifier.
      const updatedModifiers = pendingModifiers - modifier;
      this.set(
          'pendingAcceleratorInfo.layoutProperties.standardAccelerator.' +
              'accelerator.modifiers',
          updatedModifiers);
    } else {
      // Remove the key that was just released.
      const updatedAccelerator = pendingAccelerator;
      updatedAccelerator.keyCode = 0;
      this.set(
          'pendingAcceleratorInfo.layoutProperties.standardAccelerator.' +
              'accelerator',
          updatedAccelerator);
      this.set(
          'pendingAcceleratorInfo.layoutProperties.standardAccelerator' +
              '.keyDisplay',
          '');
    }
  }

  private async processPendingAccelerator(
      pendingAccelInfo: StandardAcceleratorInfo): Promise<void> {
    // Reset status state when processing the new accelerator.
    this.statusMessage = '';
    this.hasError = false;

    let result: {result: AcceleratorResultData};
    assert(this.viewState !== ViewState.VIEW);

    // If the accelerator is disabled, we should only add the new accelerator.
    const isDisabledAccelerator =
        this.acceleratorInfo.state === AcceleratorState.kDisabledByUser;

    if (this.viewState === ViewState.ADD || isDisabledAccelerator) {
      result = await this.shortcutProvider.addAccelerator(
          this.source, this.action, getAccelerator(pendingAccelInfo));
    }

    if (this.viewState === ViewState.EDIT && !isDisabledAccelerator) {
      const originalAccelerator: Accelerator|undefined =
          this.acceleratorInfo.layoutProperties.standardAccelerator
              ?.originalAccelerator;
      const acceleratorToEdit =
          originalAccelerator || getAccelerator(this.acceleratorInfo);
      result = await this.shortcutProvider.replaceAccelerator(
          this.source, this.action, acceleratorToEdit,
          getAccelerator(pendingAccelInfo));
    }
    this.handleAcceleratorResultData(result!.result);
  }

  // TODO(longbowei): Finalize and localize these messages.
  private handleAcceleratorResultData(result: AcceleratorResultData): void {
    switch (result.result) {
      // Shift is the only modifier.
      case AcceleratorConfigResult.kShiftOnlyNotAllowed: {
        this.statusMessage = this.i18n('shiftOnlyNotAllowedStatusMessage');
        this.hasError = true;
        return;
      }
      // No modifiers is pressed before primary key.
      case AcceleratorConfigResult.kMissingModifier: {
        // This is a backup check, since only valid accelerators are processed
        // and a valid accelerator will have modifier(s) and a key or is
        // function key.
        this.statusMessage = this.i18n('missingModifierStatusMessage');
        this.hasError = true;
        return;
      }
      // Top row key used as activation keys(no search key pressed).
      case AcceleratorConfigResult.kKeyNotAllowed: {
        this.statusMessage = this.i18n('keyNotAllowedStatusMessage');
        this.hasError = true;
        return;
      }
      // Search with function keys are not allowed.
      // TODO(b/286268215): localize string.
      case AcceleratorConfigResult.kSearchWithFunctionKeyNotAllowed: {
        this.statusMessage =
            this.i18n('searchWithFunctionKeyNotAllowedStatusMessage');
        this.hasError = true;
        return;
      }
      // Conflict with a locked accelerator.
      case AcceleratorConfigResult.kConflict:
      case AcceleratorConfigResult.kActionLocked: {
        this.statusMessage = this.i18n(
            'lockedShortcutStatusMessage',
            mojoString16ToString(result.shortcutName as String16));
        this.hasError = true;
        return;
      }
      // Conflict with an editable shortcut.
      case AcceleratorConfigResult.kConflictCanOverride: {
        this.statusMessage = this.i18n(
            'shortcutWithConflictStatusMessage',
            mojoString16ToString(result.shortcutName as String16));
        this.hasError = true;
        return;
      }
      // Limit to only 5 accelerators allowed.
      case AcceleratorConfigResult.kMaximumAcceleratorsReached: {
        this.statusMessage = this.i18n('maxAcceleratorsReachedHint');
        this.hasError = true;
        return;
      }
      case AcceleratorConfigResult.kNonSearchAcceleratorWarning: {
        // TODO(jimmyxgong): Add the "Learn More" link when available.
        this.statusMessage = this.i18n('warningSearchNotIncluded');
        this.hasError = true;
        return;
      }
      case AcceleratorConfigResult.kSuccess: {
        this.fireUpdateEvent();
        getShortcutProvider().recordUserAction(
            UserAction.kSuccessfulModification);
        return;
      }
    }
    assertNotReached();
  }

  /**
   * Converts a keystroke event to an Accelerator Object.
   */
  private keystrokeToAccelerator(e: KeyboardEvent): Accelerator {
    const output: Accelerator = {
      modifiers: 0,
      keyCode: 0,
      keyState: AcceleratorKeyState.PRESSED,
    };
    if (e.metaKey) {
      output.modifiers = output.modifiers | Modifier.COMMAND;
    }
    if (e.ctrlKey) {
      output.modifiers = output.modifiers | Modifier.CONTROL;
    }
    if (e.altKey) {
      output.modifiers = output.modifiers | Modifier.ALT;
    }
    // Shift key isn't registered as a modifier unless a non-modifer key is
    // pressed in conjunction with the keystroke.
    if (e.key == 'Shift' || e.shiftKey) {
      output.modifiers = output.modifiers | Modifier.SHIFT;
    }

    // Only add non-modifier or function keys as the pending key.
    if (!this.isModifierKey(e) || isFunctionKey(e.keyCode)) {
      output.keyCode = e.keyCode;
    }

    return output;
  }

  private getKeyDisplay(e: KeyboardEvent): string {
    switch (e.code) {
      case 'Space':  // Space key: e.key: ' ', e.code: 'Space', set keyDisplay
                     // to be 'space' text.
        return 'space';
      case 'ShowAllWindows':  // Overview key: e.key: 'F4', e.code:
                              // 'ShowAllWindows', set keyDisplay to be
                              // 'LaunchApplication1' and will display as
                              // 'overview' icon.
        return 'LaunchApplication1';
      case '':
        // If there is no `code`, check the `key`. If the `key` is
        // `unidentified`, we need to manually lookup the key.
        return unidentifiedKeyCodeToKey[e.keyCode] || e.key;
      default:  // All other keys: Use the original e.key as keyDisplay.
        return e.key;
    }
  }

  private isModifierKey(e: KeyboardEvent): boolean {
    return ModifierKeyCodes.includes(e.keyCode);
  }

  /**
   * Returns the specified CSS state of the modifier key element.
   */
  protected getCtrlState(): string {
    return this.getModifierState(Modifier.CONTROL);
  }

  /**
   * Returns the specified CSS state of the modifier key element.
   */
  protected getAltState(): string {
    return this.getModifierState(Modifier.ALT);
  }

  /**
   * Returns the specified CSS state of the modifier key element.
   */
  protected getShiftState(): string {
    return this.getModifierState(Modifier.SHIFT);
  }

  /**
   * Returns the specified CSS state of the modifier key element.
   */
  protected getSearchState(): string {
    return this.getModifierState(Modifier.COMMAND);
  }

  /**
   * Returns the specified CSS state of the modifier key element.
   */
  private getModifierState(modifier: Modifier): KeyState {
    // If the accelerator is disabled, we default to the `NOT_SELECTED` state if
    // the user is not editing the accelerator.
    if (this.isDisabled && this.viewState !== ViewState.EDIT) {
      return KeyState.NOT_SELECTED;
    }

    if ((getAccelerator(this.pendingAcceleratorInfo)).modifiers & modifier) {
      return KeyState.MODIFIER;
    }
    return KeyState.NOT_SELECTED;
  }

  /**
   * Returns the specified CSS state of the pending key element.
   */
  protected getPendingKeyState(): string {
    if (this.pendingAcceleratorInfo.layoutProperties.standardAccelerator
            .keyDisplay != '') {
      return KeyState.ALPHANUMERIC;
    }
    return KeyState.NOT_SELECTED;
  }

  /**
   * Returns the specified key to display.
   */
  protected getPendingKey(): string {
    if (this.pendingAcceleratorInfo.layoutProperties.standardAccelerator
            .keyDisplay != '') {
      const keyDisplay = this.pendingAcceleratorInfo.layoutProperties
                             .standardAccelerator.keyDisplay;
      // Display as icon if it exists in the map.
      if (keyDisplay in keyToIconNameMap) {
        return keyDisplay;
      }
      return keyDisplay.toLowerCase();
    }
    // TODO(jimmyxgong): Reset to a localized default empty state.
    return 'key';
  }

  /**
   * Returns true if the event has valid modifiers.
   */
  private hasValidModifiers(e: KeyboardEvent): boolean {
    // Although Shift is a modifier, it cannot be a standalone modifier for a
    // shortcut.
    return e.ctrlKey || e.altKey || e.metaKey;
  }

  private isValidDefaultAccelerator(accelInfo: StandardAcceleratorInfo):
      boolean {
    // A valid default accelerator is one that has modifier(s) and a key or
    // is function key.
    const accelerator =
        accelInfo.layoutProperties.standardAccelerator.accelerator;
    return (accelerator.modifiers > 0 &&
            accelInfo.layoutProperties.standardAccelerator.keyDisplay !== '') ||
        isFunctionKey(accelerator.keyCode);
  }

  private showEditView(): boolean {
    return this.viewState !== ViewState.VIEW;
  }

  private fireUpdateEvent(): void {
    if (this.acceleratorInfo.state === AcceleratorState.kDisabledByUser &&
        isStandardAcceleratorInfo(this.acceleratorInfo)) {
      this.dispatchEvent(new CustomEvent('default-conflict-resolved', {
        bubbles: true,
        composed: true,
        detail: {
          stringifiedAccelerator:
              JSON.stringify(getAccelerator(this.acceleratorInfo)),
        },
      }));
    }

    // Always end input capturing if an update event was fired.
    this.endCapture(/*should_delay=*/ true);

    setTimeout(() => {
      this.dispatchEvent(new CustomEvent('request-update-accelerator', {
        bubbles: true,
        composed: true,
        detail: {source: this.source, action: this.action},
      }));
    }, kAnimationTimeoutMs);
  }

  private shouldShowLockIcon(): boolean {
    // Do not show lock icon in each row if customization is disabled or its
    // category is locked.
    if (!isCustomizationAllowed() || this.categoryIsLocked) {
      return false;
    }
    // Show lock icon if accelerator is locked.
    return (this.acceleratorInfo && this.acceleratorInfo.locked) ||
        this.sourceIsLocked;
  }

  private shouldShowEditIcon(): boolean {
    // Do not show edit icon in each row if customization is disabled, the row
    // is displayed in edit-dialog(!showEditIcon) or category is locked.
    if (!isCustomizationAllowed() || !this.showEditIcon ||
        this.categoryIsLocked) {
      return false;
    }
    // Show edit icon if accelerator is not locked.
    return !(this.acceleratorInfo && this.acceleratorInfo.locked) &&
        !this.sourceIsLocked && this.isFirstAccelerator;
  }

  private onEditIconClicked(): void {
    this.dispatchEvent(
        new CustomEvent('edit-icon-clicked', {bubbles: true, composed: true}));
  }

  private getAriaLabel(): string {
    let keyOrIcon =
        this.acceleratorInfo.layoutProperties.standardAccelerator.keyDisplay;
    const metaKeyAriaLabel = this.lookupManager.getHasLauncherButton() ?
        this.i18n('iconLabelOpenLauncher') :
        this.i18n('iconLabelOpenSearch');
    // LWIN_KEY is not a modifier, but it is displayed as a meta icon.
    keyOrIcon = keyOrIcon === LWIN_KEY ? metaKeyAriaLabel : keyOrIcon;
    const modifiers =
        getModifiersForAcceleratorInfo(this.acceleratorInfo)
            .map(
                // Update modifiers if it includes META_KEY.
                modifier =>
                    modifier === META_KEY ? metaKeyAriaLabel : modifier);

    return [...modifiers, getKeyDisplay(keyOrIcon)].join(' ');
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  private computeIsDisabled(): boolean {
    return this.acceleratorInfo.state === AcceleratorState.kDisabledByUser ||
        this.acceleratorInfo.state === AcceleratorState.kDisabledByConflict;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'accelerator-view': AcceleratorViewElement;
  }
}

customElements.define(AcceleratorViewElement.is, AcceleratorViewElement);
