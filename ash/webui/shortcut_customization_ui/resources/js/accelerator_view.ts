// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './input_key.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AcceleratorLookupManager} from './accelerator_lookup_manager.js';
import {getTemplate} from './accelerator_view.html.js';
import {getShortcutProvider} from './mojo_interface_provider.js';
import {ModifierKeyCodes} from './shortcut_input.js';
import {Accelerator, AcceleratorConfigResult, AcceleratorSource, DefaultAcceleratorInfo, Modifier, ShortcutProviderInterface} from './shortcut_types.js';
import {areAcceleratorsEqual, createEmptyAcceleratorInfo, getAccelerator} from './shortcut_utils.js';

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

/**
 * Returns the converted modifier flag as a readable string.
 * TODO(jimmyxgong): Localize, replace with icon, or update strings.
 */
function getModifierString(modifier: Modifier): string {
  switch (modifier) {
    case Modifier.SHIFT:
      return 'shift';
    case Modifier.CONTROL:
      return 'ctrl';
    case Modifier.ALT:
      return 'alt';
    case Modifier.COMMAND:
      return 'meta';
    default:
      return '';
  }
}

/**
 * @fileoverview
 * 'accelerator-view' is wrapper component for an accelerator. It maintains both
 * the read-only and editable state of an accelerator.
 * TODO(jimmyxgong): Implement the edit mode.
 */
export class AcceleratorViewElement extends PolymerElement {
  static get is() {
    return 'accelerator-view';
  }

  static get properties() {
    return {
      acceleratorInfo: {
        type: Object,
        value: () => {},
      },

      pendingAcceleratorInfo_: {
        type: Object,
        value: () => {},
      },

      acceleratorOnHold_: {
        type: String,
        value: '',
      },

      viewState: {
        type: Number,
        value: ViewState.VIEW,
        notify: true,
        observer: 'onViewStateChanged_',
      },

      modifiers_: {
        type: Array,
        computed: 'getModifiers_(acceleratorInfo.accelerator.*)',
      },

      isCapturing_: {
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
    };
  }

  acceleratorInfo: DefaultAcceleratorInfo;
  viewState: ViewState;
  statusMessage: string;
  hasError: boolean;
  action: number;
  source: AcceleratorSource;
  protected pendingAcceleratorInfo_: DefaultAcceleratorInfo;
  private modifiers_: string[];
  private acceleratorOnHold_: string;
  private isCapturing_: boolean;
  private shortcutProvider_: ShortcutProviderInterface = getShortcutProvider();
  private lookupManager_: AcceleratorLookupManager =
      AcceleratorLookupManager.getInstance();

  private getModifiers_(): string[] {
    const modifiers: Modifier[] = [
      Modifier.SHIFT,
      Modifier.CONTROL,
      Modifier.ALT,
      Modifier.COMMAND,
    ];
    const modifierStrings: string[] = [];
    for (const modifier of modifiers) {
      if ((getAccelerator(this.acceleratorInfo)).modifiers & modifier) {
        modifierStrings.push(getModifierString(modifier));
      }
    }
    return modifierStrings;
  }

  protected onViewStateChanged_() {
    if (this.viewState !== ViewState.VIEW) {
      this.registerKeyEventListeners_();
      return;
    }
    this.unregisterKeyEventListeners_();
  }

  private registerKeyEventListeners_() {
    this.addEventListener('keydown', (e) => this.onKeyDown_(e));
    this.addEventListener('keyup', (e) => this.onKeyUp_(e));
    this.addEventListener('focus', () => this.startCapture_());
    this.addEventListener('mouseup', () => this.startCapture_());
    this.addEventListener('blur', () => this.endCapture_());
    this.$.container.focus();
  }

  private unregisterKeyEventListeners_() {
    this.removeEventListener('keydown', (e) => this.onKeyDown_(e));
    this.removeEventListener('keyup', (e) => this.onKeyUp_(e));
    this.removeEventListener('focus', () => this.startCapture_());
    this.removeEventListener('mouseup', () => this.startCapture_());
    this.removeEventListener('blur', () => this.endCapture_());
  }


  private startCapture_() {
    if (this.isCapturing_) {
      return;
    }
    // Disable ChromeOS accelerator handler when starting input capture.
    this.pendingAcceleratorInfo_ = createEmptyAcceleratorInfo();
    this.isCapturing_ = true;

    this.dispatchEvent(new CustomEvent('accelerator-capturing-started', {
      bubbles: true,
      composed: true,
    }));
  }

  private endCapture_() {
    if (!this.isCapturing_) {
      return;
    }

    this.viewState = ViewState.VIEW;
    this.statusMessage = '';
    this.hasError = false;
    this.isCapturing_ = false;
    this.pendingAcceleratorInfo_ = createEmptyAcceleratorInfo();

    this.dispatchEvent(new CustomEvent('accelerator-capturing-ended', {
      bubbles: true,
      composed: true,
    }));
  }

  private onKeyDown_(e: KeyboardEvent) {
    this.handleKey_(e);
  }

  private onKeyUp_(e: KeyboardEvent) {
    e.preventDefault();
    e.stopPropagation();
    // TODO(jimmyxgong): Check for errors e.g. accelerator conflicts.
  }

  private handleKey_(e: KeyboardEvent) {
    // While capturing, we prevent all events from bubbling, to prevent
    // shortcuts from executing and interrupting the input capture.
    e.preventDefault();
    e.stopPropagation();

    if (!this.hasValidModifiers_(e)) {
      // TODO(jimmyxgong): Fire events for error handling, e.g. Shift cannot be
      // the only modifier.
      this.pendingAcceleratorInfo_ = createEmptyAcceleratorInfo();
      return;
    }
    this.set(
        'pendingAcceleratorInfo_.layoutProperties.defaultAccelerator.accelerator',
        this.keystrokeToAccelerator_(e));

    if (this.isModifierKey_(e)) {
      // Reset the keyDisplay property if the key is a modifier.
      this.set(
          'pendingAcceleratorInfo_.layoutProperties.defaultAccelerator.keyDisplay',
          '');
    } else {
      this.set(
          'pendingAcceleratorInfo_.layoutProperties.defaultAccelerator.keyDisplay',
          e.key);
    }

    // New shortcut matches the current shortcut, end capture.
    if (areAcceleratorsEqual(
            getAccelerator(this.pendingAcceleratorInfo_),
            this.acceleratorInfo.layoutProperties.defaultAccelerator
                .accelerator)) {
      this.endCapture_();
      return;
    }

    // Only process valid accelerators.
    if (this.isValidDefaultAccelerator_(this.pendingAcceleratorInfo_)) {
      this.processPendingAccelerator_(this.pendingAcceleratorInfo_);
    }
  }

  /**
   * Checks that |pendingAccelerator_| is not a pre-existing shortcut. Sets the
   * error message if there is a conflict.
   */
  private processPendingAccelerator_(pendingAccelInfo: DefaultAcceleratorInfo) {
    // Reset status state when processing the new accelerator.
    this.statusMessage = '';
    this.hasError = false;

    // If |acceleratorOnHold_| is not empty then the user has attempted to
    // replace a pre-existing accelerator. Check that the new accelerator
    // matches the |acceleratorOnHold_|, otherwise reset its value.
    if (this.acceleratorOnHold_ !== '') {
      if (this.acceleratorOnHold_ ===
          JSON.stringify(getAccelerator(pendingAccelInfo))) {
        // User re-pressed the shortcut, send a request to replace the
        // accelerator.
        this.requestUpdateAccelerator_(pendingAccelInfo);
        return;
      }
      this.acceleratorOnHold_ = '';
    }

    const foundId = this.lookupManager_.getAcceleratorIdFromReverseLookup(
        getAccelerator(pendingAccelInfo));

    // Pre-existing shortcut, update the error message.
    if (foundId !== undefined) {
      // TODO(jimmyxgong): Fetch name of accelerator with real implementation.
      const uuidParams = foundId.split('-');
      const conflictSource: AcceleratorSource = parseInt(uuidParams[0], 10);
      const conflictAction = parseInt(uuidParams[1], 10);
      const conflictAccelName = this.lookupManager_.getAcceleratorName(
          conflictSource, conflictAction);

      // Cannot override a locked action.
      if (!this.shortcutProvider_.isMutable(conflictSource) ||
          this.lookupManager_.isAcceleratorLocked(
              conflictSource, conflictAction,
              getAccelerator(pendingAccelInfo))) {
        // TODO(jimmyxgong): i18n this string.
        this.statusMessage = 'Shortcut is used by \"' + conflictAccelName +
            '\". Press a new shortcut to replace.';
        this.hasError = true;
        return;
      }

      // TODO(jimmyxgong): i18n this string.
      this.statusMessage = 'Shortcut is used by ' + conflictAccelName +
          '. Press a new shortcut or press the same one again to use it for ' +
          'this action instead.';
      this.hasError = true;

      // Store the pending accelerator.
      this.acceleratorOnHold_ =
          JSON.stringify(this.pendingAcceleratorInfo_.layoutProperties
                             .defaultAccelerator.accelerator);
      return;
    }

    // No conflicts, request replacement.
    this.requestUpdateAccelerator_(pendingAccelInfo);
  }

  /**
   * Converts a keystroke event to an Accelerator Object.
   */
  private keystrokeToAccelerator_(e: KeyboardEvent): Accelerator {
    const output: Accelerator = {modifiers: 0, keyCode: 0};
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

    // Only add non-modifier keys as the pending key.
    if (!this.isModifierKey_(e)) {
      output.keyCode = e.keyCode;
    }

    return output;
  }

  private isModifierKey_(e: KeyboardEvent): boolean {
    return ModifierKeyCodes.includes(e.keyCode);
  }

  /**
   * Returns the specified CSS state of the modifier key element.
   */
  protected getCtrlState_(): string {
    return this.getModifierState_(Modifier.CONTROL);
  }

  /**
   * Returns the specified CSS state of the modifier key element.
   */
  protected getAltState_(): string {
    return this.getModifierState_(Modifier.ALT);
  }

  /**
   * Returns the specified CSS state of the modifier key element.
   */
  protected getShiftState_(): string {
    return this.getModifierState_(Modifier.SHIFT);
  }

  /**
   * Returns the specified CSS state of the modifier key element.
   */
  protected getSearchState_(): string {
    return this.getModifierState_(Modifier.COMMAND);
  }

  /**
   * Returns the specified CSS state of the modifier key element.
   */
  private getModifierState_(modifier: Modifier): KeyState {
    if ((getAccelerator(this.pendingAcceleratorInfo_)).modifiers & modifier) {
      return KeyState.MODIFIER;
    }
    return KeyState.NOT_SELECTED;
  }

  /**
   * Returns the specified CSS state of the pending key element.
   */
  protected getPendingKeyState_(): string {
    if (this.pendingAcceleratorInfo_.layoutProperties.defaultAccelerator
            .keyDisplay != '') {
      return KeyState.ALPHANUMERIC;
    }
    return KeyState.NOT_SELECTED;
  }

  /**
   * Returns the specified key to display.
   */
  protected getPendingKey_(): string {
    if (this.pendingAcceleratorInfo_.layoutProperties.defaultAccelerator
            .keyDisplay != '') {
      return this.pendingAcceleratorInfo_.layoutProperties.defaultAccelerator
          .keyDisplay.toLowerCase();
    }
    // TODO(jimmyxgong): Reset to a localized default empty state.
    return 'key';
  }

  /**
   * Returns true if the event has valid modifiers.
   */
  private hasValidModifiers_(e: KeyboardEvent): boolean {
    // Although Shift is a modifier, it cannot be a standalone modifier for a
    // shortcut.
    return e.ctrlKey || e.altKey || e.metaKey;
  }

  private isValidDefaultAccelerator_(accelInfo: DefaultAcceleratorInfo):
      boolean {
    // A valid default accelerator is on that has modifier(s) and a key.
    return accelInfo.layoutProperties.defaultAccelerator.accelerator.modifiers >
        0 &&
        accelInfo.layoutProperties.defaultAccelerator.keyDisplay !== '';
  }

  private showEditView_(): boolean {
    return this.viewState !== ViewState.VIEW;
  }

  private requestUpdateAccelerator_(newAcceleratorInfo:
                                        DefaultAcceleratorInfo) {
    if (this.viewState === ViewState.EDIT) {
      this.shortcutProvider_
          .replaceAccelerator(
              this.source, this.action, (getAccelerator(this.acceleratorInfo)),
              (getAccelerator(newAcceleratorInfo)))
          .then((result: AcceleratorConfigResult) => {
            // TODO(jimmyxgong): Handle other error cases.
            if (result === AcceleratorConfigResult.SUCCESS) {
              this.lookupManager_.replaceAccelerator(
                  this.source, this.action,
                  this.acceleratorInfo.layoutProperties.defaultAccelerator
                      .accelerator,
                  newAcceleratorInfo);
              this.fireUpdateEvent_();
            }
          });
    }

    if (this.viewState === ViewState.ADD) {
      this.shortcutProvider_
          .addUserAccelerator(
              this.source, this.action, getAccelerator(newAcceleratorInfo))
          .then((result: AcceleratorConfigResult) => {
            // TODO(jimmyxgong): Handle other error cases.
            if (result === AcceleratorConfigResult.SUCCESS) {
              this.lookupManager_.addAccelerator(
                  this.source, this.action, newAcceleratorInfo);
              this.fireUpdateEvent_();
            }
          });
    }
  }

  private fireUpdateEvent_() {
    this.dispatchEvent(new CustomEvent('request-update-accelerator', {
      bubbles: true,
      composed: true,
      detail: {source: this.source, action: this.action},
    }));

    // Always end input capturing if an update event was fired.
    this.endCapture_();
  }

  static get template() {
    return getTemplate();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'accelerator-view': AcceleratorViewElement;
  }
}

customElements.define(AcceleratorViewElement.is, AcceleratorViewElement);
