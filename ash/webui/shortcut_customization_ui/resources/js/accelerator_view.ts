// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './input_key.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert_ts.js';
import {String16} from 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AcceleratorResultData} from '../mojom-webui/ash/webui/shortcut_customization_ui/mojom/shortcut_customization.mojom-webui.js';

import {AcceleratorLookupManager} from './accelerator_lookup_manager.js';
import {getTemplate} from './accelerator_view.html.js';
import {getShortcutProvider} from './mojo_interface_provider.js';
import {mojoString16ToString} from './mojo_utils.js';
import {ModifierKeyCodes} from './shortcut_input.js';
import {Accelerator, AcceleratorConfigResult, AcceleratorSource, Modifier, ShortcutProviderInterface, StandardAcceleratorInfo} from './shortcut_types.js';
import {areAcceleratorsEqual, createEmptyAcceleratorInfo, getAccelerator, getModifiersForAcceleratorInfo, isCustomizationDisabled} from './shortcut_utils.js';

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

      acceleratorOnHold: {
        type: String,
        value: '',
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
    };
  }

  acceleratorInfo: StandardAcceleratorInfo;
  viewState: ViewState;
  statusMessage: string;
  hasError: boolean;
  action: number;
  source: AcceleratorSource;
  sourceIsLocked: boolean;
  protected pendingAcceleratorInfo: StandardAcceleratorInfo;
  private modifiers: string[];
  private acceleratorOnHold: string;
  private isCapturing: boolean;
  private shortcutProvider: ShortcutProviderInterface = getShortcutProvider();
  private lookupManager: AcceleratorLookupManager =
      AcceleratorLookupManager.getInstance();

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
    this.addEventListener('keydown', (e) => this.onKeyDown(e));
    this.addEventListener('keyup', (e) => this.onKeyUp(e));
    this.addEventListener('focus', () => this.startCapture());
    this.addEventListener('mouseup', () => this.startCapture());
    this.addEventListener('blur', () => this.endCapture());
    this.$.container.focus();
  }

  private unregisterKeyEventListeners(): void {
    this.removeEventListener('keydown', (e) => this.onKeyDown(e));
    this.removeEventListener('keyup', (e) => this.onKeyUp(e));
    this.removeEventListener('focus', () => this.startCapture());
    this.removeEventListener('mouseup', () => this.startCapture());
    this.removeEventListener('blur', () => this.endCapture());
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

  private async endCapture(): Promise<void> {
    if (!this.isCapturing) {
      return;
    }

    this.viewState = ViewState.VIEW;
    this.statusMessage = '';
    this.hasError = false;
    this.isCapturing = false;
    this.pendingAcceleratorInfo = createEmptyAcceleratorInfo();

    this.dispatchEvent(new CustomEvent('accelerator-capturing-ended', {
      bubbles: true,
      composed: true,
    }));

    await this.shortcutProvider.preventProcessingAccelerators(false);
  }

  private onKeyDown(e: KeyboardEvent): void {
    this.handleKey(e);
  }

  private onKeyUp(e: KeyboardEvent): void {
    e.preventDefault();
    e.stopPropagation();
    // TODO(jimmyxgong): Check for errors e.g. accelerator conflicts.
  }

  private handleKey(e: KeyboardEvent): void {
    // While capturing, we prevent all events from bubbling, to prevent
    // shortcuts from executing and interrupting the input capture.
    e.preventDefault();
    e.stopPropagation();

    if (!this.hasValidModifiers(e)) {
      // TODO(jimmyxgong): Fire events for error handling, e.g. Shift cannot be
      // the only modifier.
      this.pendingAcceleratorInfo = createEmptyAcceleratorInfo();
      return;
    }
    this.set(
        'pendingAcceleratorInfo.layoutProperties.standardAccelerator.accelerator',
        this.keystrokeToAccelerator(e));

    if (this.isModifierKey(e)) {
      // Reset the keyDisplay property if the key is a modifier.
      this.set(
          'pendingAcceleratorInfo.layoutProperties.standardAccelerator.keyDisplay',
          '');
    } else {
      this.set(
          'pendingAcceleratorInfo.layoutProperties.standardAccelerator.keyDisplay',
          e.key);
    }

    // New shortcut matches the current shortcut, end capture.
    if (areAcceleratorsEqual(
            getAccelerator(this.pendingAcceleratorInfo),
            this.acceleratorInfo.layoutProperties.standardAccelerator
                .accelerator)) {
      this.endCapture();
      return;
    }

    // Only process valid accelerators.
    if (this.isValidDefaultAccelerator(this.pendingAcceleratorInfo)) {
      this.processPendingAccelerator(this.pendingAcceleratorInfo);
    }
  }

  private async processPendingAccelerator(
      pendingAccelInfo: StandardAcceleratorInfo): Promise<void> {
    // Reset status state when processing the new accelerator.
    this.statusMessage = '';
    this.hasError = false;

    let result: {result: AcceleratorResultData};
    assert(this.viewState !== ViewState.VIEW);

    if (this.viewState === ViewState.ADD) {
      result = await this.shortcutProvider.addAccelerator(
          this.source, this.action, getAccelerator(pendingAccelInfo));
    }

    if (this.viewState === ViewState.EDIT) {
      result = await this.shortcutProvider.replaceAccelerator(
          this.source, this.action, getAccelerator(this.acceleratorInfo),
          getAccelerator(pendingAccelInfo));
    }
    this.handleAcceleratorResultData(result!.result);
  }

  private handleAcceleratorResultData(result: AcceleratorResultData): void {
    switch (result.result) {
      case AcceleratorConfigResult.kConflict:
      case AcceleratorConfigResult.kActionLocked: {
        this.statusMessage = this.i18n(
            'lockedShortcutStatusMessage',
            mojoString16ToString(result.shortcutName as String16));
        this.hasError = true;
        return;
      }
      case AcceleratorConfigResult.kShiftOnlyNotAllowed:
      case AcceleratorConfigResult.kMissingModifier: {
        // TODO(jimmyxgong): Replace and localize this string.
        this.statusMessage = 'Bad modifiers"';
        this.hasError = true;
        return;
      }
      case AcceleratorConfigResult.kConflictCanOverride: {
        this.statusMessage = this.i18n(
            'shortcutWithConflictStatusMessage',
            mojoString16ToString(result.shortcutName as String16));
        this.hasError = true;
        return;
      }
      case AcceleratorConfigResult.kSuccess: {
        this.pendingAcceleratorInfo = createEmptyAcceleratorInfo();
        this.fireUpdateEvent();
        return;
      }
    }
    assertNotReached();
  }

  /**
   * Converts a keystroke event to an Accelerator Object.
   */
  private keystrokeToAccelerator(e: KeyboardEvent): Accelerator {
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
    if (!this.isModifierKey(e)) {
      output.keyCode = e.keyCode;
    }

    return output;
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
      return this.pendingAcceleratorInfo.layoutProperties.standardAccelerator
          .keyDisplay.toLowerCase();
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
    // A valid default accelerator is on that has modifier(s) and a key.
    return accelInfo.layoutProperties.standardAccelerator.accelerator
               .modifiers > 0 &&
        accelInfo.layoutProperties.standardAccelerator.keyDisplay !== '';
  }

  private showEditView(): boolean {
    return this.viewState !== ViewState.VIEW;
  }

  private fireUpdateEvent(): void {
    this.dispatchEvent(new CustomEvent('request-update-accelerator', {
      bubbles: true,
      composed: true,
      detail: {source: this.source, action: this.action},
    }));

    // Always end input capturing if an update event was fired.
    this.endCapture();
  }

  private shouldShowLockIcon(): boolean {
    if (isCustomizationDisabled()) {
      return false;
    }

    return (this.acceleratorInfo && this.acceleratorInfo.locked) ||
        this.sourceIsLocked;
  }

  /**
   * Determines whether accelerator items should be tab-focusable.
   */
  private getTabIndex(): number {
    // If customization is disabled, this element should not be tab-focusable.
    return isCustomizationDisabled() ? -1 : 0;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'accelerator-view': AcceleratorViewElement;
  }
}

customElements.define(AcceleratorViewElement.is, AcceleratorViewElement);
