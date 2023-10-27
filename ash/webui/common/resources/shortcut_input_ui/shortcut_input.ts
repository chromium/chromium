// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shortcut_input_key.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assertNotReached} from 'chrome://resources/js/assert.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {FakeShortcutInputProvider} from './fake_shortcut_input_provider.js';
import {KeyEvent} from './input_device_settings.mojom-webui.js';
import {getTemplate} from './shortcut_input.html.js';
import {ShortcutInputObserverReceiver, ShortcutInputProviderInterface} from './shortcut_input_provider.mojom-webui.js';
import {AllowedModifierKeyCodes, getSortedModifiers, KeyInputState, KeyToIconNameMap, Modifier, ModifierKeyCodes, Modifiers} from './shortcut_utils.js';

export interface ShortcutInputElement {
  $: {
    container: HTMLDivElement,
  };
}

/**
 * @fileoverview
 * 'shortcut-input' is wrapper component for a key event. It maintains both
 * the read-only and editable state of a key event.
 */
const ShortcutInputElementBase = I18nMixin(PolymerElement);

export class ShortcutInputElement extends ShortcutInputElementBase {
  static get is(): string {
    return 'shortcut-input';
  }

  static get properties(): PolymerElementProperties {
    return {
      pendingKeyEvent: {type: Object},

      shortcutInputProvider: {type: Object},

      modifiers: {
        type: Array,
        computed: 'getModifiers(pendingKeyEvent)',
        value: [],
      },

      showSeparator: {
        type: Boolean,
      },
    };
  }

  hasLauncherButton: boolean = true;
  shortcutInputProvider: ShortcutInputProviderInterface|null = null;
  pendingKeyEvent: KeyEvent|null = null;
  modifiers: Modifier[] = [];
  showSeparator: boolean = false;
  isCapturing: boolean = false;
  private shortcutInputObserverReceiver: ShortcutInputObserverReceiver|null =
      null;
  private eventTracker: EventTracker = new EventTracker();

  private observeShortcutInput(): void {
    if (!this.shortcutInputProvider) {
      return;
    }

    if (this.shortcutInputProvider instanceof FakeShortcutInputProvider) {
      this.shortcutInputProvider.startObservingShortcutInput(this);
      return;
    }

    this.shortcutInputObserverReceiver =
        new ShortcutInputObserverReceiver(this);
    this.shortcutInputProvider.startObservingShortcutInput(
        this.shortcutInputObserverReceiver.$.bindNewPipeAndPassRemote());
  }

  /**
   * Updates UI to the newly received KeyEvent.
   */
  onShortcutInputEventPressed(event: KeyEvent): void {
    this.pendingKeyEvent = event;
  }

  /**
   * Updates the UI to the new KeyEvent and dispatches and event to notify
   * parent elements.
   */
  onShortcutInputEventReleased(event: KeyEvent): void {
    // Only update the UI if the released key is the last key pressed OR if its
    // a modifier.
    if (this.pendingKeyEvent && event.vkey !== this.pendingKeyEvent.vkey) {
      if (!ModifierKeyCodes.includes(event.vkey as number)) {
        return;
      }

      this.pendingKeyEvent.modifiers = event.modifiers;
      return;
    }

    this.pendingKeyEvent = event;
    this.dispatchEvent(new CustomEvent('shortcut-input-event', {
      bubbles: true,
      composed: true,
      detail: {
        keyEvent: this.pendingKeyEvent,
      },
    }));
  }

  override connectedCallback(): void {
    super.connectedCallback();
  }

  override disconnectedCallback(): void {
    super.disconnectedCallback();
    this.eventTracker.removeAll();
  }

  /**
   * shortcut_input only starts observing via calling `startObserving`. It
   * registers event handlers so it stops observing input when focus is lost.
   */
  startObserving(): void {
    this.isCapturing = true;
    this.observeShortcutInput();
    this.eventTracker.add(
        this, 'keydown', (e: KeyboardEvent) => this.stopEvent(e));
    this.eventTracker.add(
        this, 'keyup', (e: KeyboardEvent) => this.stopEvent(e));
    this.eventTracker.add(this, 'blur', () => this.stopObserving());
    this.$.container.focus();
    this.dispatchCaptureStateEvent();
  }

  stopObserving(): void {
    this.isCapturing = false;
    this.shortcutInputProvider?.stopObservingShortcutInput();
    this.eventTracker.removeAll();
    this.dispatchCaptureStateEvent();
  }

  reset(): void {
    this.pendingKeyEvent = null;
  }

  /**
   * Consumes all events received by the shortcut_input element.
   */
  private stopEvent(e: KeyboardEvent): void {
    e.preventDefault();
    e.stopPropagation();
  }

  getKey(): string {
    if (this.pendingKeyEvent && this.pendingKeyEvent.keyDisplay != '') {
      const keyDisplay = this.pendingKeyEvent.keyDisplay;
      if (keyDisplay in KeyToIconNameMap) {
        return keyDisplay;
      }
      return keyDisplay.toLowerCase();
    }
    // TODO(dpad, b/286930911): Reset to localized default empty state.
    return 'key';
  }

  getKeyState(): string {
    if (this.pendingKeyEvent && this.pendingKeyEvent.keyDisplay != '') {
      return KeyInputState.ALPHANUMERIC_SELECTED;
    }
    return KeyInputState.NOT_SELECTED;
  }

  shouldShowEditView(): boolean {
    return this.isCapturing;
  }

  shouldShowConfirmView(): boolean {
    return this.pendingKeyEvent !== null && !this.isCapturing;
  }

  /**
   * Returns the specified CSS state of the modifier key element.
   */
  protected getCtrlState(): string {
    if (this.pendingKeyEvent?.vkey as number === AllowedModifierKeyCodes.CTRL) {
      return KeyInputState.NOT_SELECTED;
    }
    return this.getModifierState(Modifier.CONTROL);
  }

  /**
   * Returns the specified CSS state of the modifier key element.
   */
  protected getAltState(): string {
    if (this.pendingKeyEvent?.vkey as number === AllowedModifierKeyCodes.ALT) {
      return KeyInputState.NOT_SELECTED;
    }
    return this.getModifierState(Modifier.ALT);
  }

  /**
   * Returns the specified CSS state of the modifier key element.
   */
  protected getShiftState(): string {
    if (this.pendingKeyEvent?.vkey as number ===
        AllowedModifierKeyCodes.SHIFT) {
      return KeyInputState.NOT_SELECTED;
    }
    return this.getModifierState(Modifier.SHIFT);
  }

  /**
   * Returns the specified CSS state of the modifier key element.
   */
  protected getSearchState(): string {
    if (this.pendingKeyEvent?.vkey as number ===
            AllowedModifierKeyCodes.META_LEFT ||
        this.pendingKeyEvent?.vkey as number ===
            AllowedModifierKeyCodes.META_RIGHT) {
      return KeyInputState.NOT_SELECTED;
    }
    return this.getModifierState(Modifier.COMMAND);
  }

  /**
   * Returns the specified CSS state of the modifier key element.
   */
  private getModifierState(modifier: Modifier): KeyInputState {
    if (this.pendingKeyEvent && this.pendingKeyEvent?.modifiers & modifier) {
      return KeyInputState.MODIFIER_SELECTED;
    }

    return KeyInputState.NOT_SELECTED;
  }

  private getModifierString(modifier: Modifier): string {
    switch (modifier) {
      case Modifier.SHIFT:
        return 'shift';
      case Modifier.CONTROL:
        return 'ctrl';
      case Modifier.ALT:
        return 'alt';
      case Modifier.COMMAND:
        return 'meta';
    }
    return assertNotReached();
  }

  /**
   * Returns a list of the modifier strings for the held down modifiers within
   * `keyEvent.`
   */
  getModifiers(keyEvent: KeyEvent): string[] {
    if (!keyEvent) {
      return [];
    }
    const modifierStrings: string[] = [];
    for (const modifier of Modifiers) {
      if (keyEvent.modifiers & modifier) {
        modifierStrings.push(this.getModifierString(modifier));
      }
    }
    return getSortedModifiers(modifierStrings);
  }

  shouldShowSeparator(): boolean {
    return this.showSeparator && this.modifiers.length > 0;
  }

  shouldShowSelectedKey() {
    return this.pendingKeyEvent !== null;
  }

  private dispatchCaptureStateEvent() {
    this.dispatchEvent(new CustomEvent('shortcut-input-capture-state', {
      bubbles: true,
      composed: true,
      detail: {
        capturing: this.isCapturing,
      },
    }));
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'shortcut-input': ShortcutInputElement;
  }
}

customElements.define(ShortcutInputElement.is, ShortcutInputElement);
