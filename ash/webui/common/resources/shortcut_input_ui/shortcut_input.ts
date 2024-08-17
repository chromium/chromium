// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shortcut_input_key.js';
import 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';

import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {assertNotReached} from 'chrome://resources/js/assert.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {FakeShortcutInputProvider} from './fake_shortcut_input_provider.js';
import {KeyEvent} from './input_device_settings.mojom-webui.js';
import {getTemplate} from './shortcut_input.html.js';
import {ShortcutInputObserverReceiver, ShortcutInputProviderInterface} from './shortcut_input_provider.mojom-webui.js';
import {getSortedModifiers, KeyInputState, KeyToIconNameMap, MetaKey, Modifier, ModifierKeyCodes, Modifiers} from './shortcut_utils.js';

// <if expr="_google_chrome" >
import {KeyToInternalIconNameMap} from './shortcut_utils.js';
// </if>

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
      // Event after event rewrites.
      pendingKeyEvent: {type: Object},

      // Event before event rewrites.
      pendingPrerewrittenKeyEvent: {type: Object},

      shortcutInputProvider: {type: Object},

      modifiers: {
        type: Array,
        computed: 'getModifiers(pendingKeyEvent, pendingPrerewrittenKeyEvent)',
        value: [],
      },

      showSeparator: {
        type: Boolean,
      },

      metaKey: Object,

      // When `updateOnKeyPress` is true, always show edit-view and and updates
      // occur on key press events rather than on key release.
      updateOnKeyPress: {
        type: Boolean,
        value: false,
      },

      // If true, will display the `pendingPrerewrittenKeyEvents` instead of
      // `pendingKeyEvent`.
      displayPrerewrittenKeyEvents: {
        type: Boolean,
      },

      // If true, this element will continue to observe for inputs even after
      // an `on-blur`. Allows parent element to handle blur events.
      ignoreBlur: {
        type: Boolean,
      },

      // If true, `onShortcutInputEventPressed` will be a no-op.
      shouldIgnoreKeyRelease: {
        type: Boolean,
      },

      hasFunctionKey: {
        type: Boolean,
      },

    };
  }

  metaKey: MetaKey = MetaKey.kSearch;
  hasFunctionKey: boolean = false;
  shortcutInputProvider: ShortcutInputProviderInterface|null = null;
  pendingKeyEvent: KeyEvent|null = null;
  pendingPrerewrittenKeyEvent: KeyEvent|null = null;
  modifiers: Modifier[] = [];
  showSeparator: boolean = false;
  isCapturing: boolean = false;
  updateOnKeyPress: boolean = false;
  displayPrerewrittenKeyEvents: boolean = false;
  ignoreBlur: boolean = false;
  shouldIgnoreKeyRelease: boolean = false;
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
  onShortcutInputEventPressed(
      prerewrittenKeyEvent: KeyEvent, keyEvent: KeyEvent|null): void {
    if (keyEvent === null) {
      if (this.displayPrerewrittenKeyEvents) {
        this.pendingKeyEvent = prerewrittenKeyEvent;
        this.pendingPrerewrittenKeyEvent = prerewrittenKeyEvent;
      } else {
        return;
      }
    } else {
      this.pendingKeyEvent = keyEvent;
      this.pendingPrerewrittenKeyEvent = prerewrittenKeyEvent;
    }

    if (this.updateOnKeyPress) {
      this.dispatchEvent(new CustomEvent('shortcut-input-event', {
        bubbles: true,
        composed: true,
        detail: {
          keyEvent: this.pendingKeyEvent,
        },
      }));
    }
  }

  /**
   * Updates the UI to the new KeyEvent and dispatches and event to notify
   * parent elements.
   */
  onShortcutInputEventReleased(
      prerewrittenKeyEvent: KeyEvent, keyEvent: KeyEvent|null): void {
    if (this.shouldIgnoreKeyRelease) {
      return;
    }

    if (keyEvent === null) {
      if (this.displayPrerewrittenKeyEvents) {
        keyEvent = prerewrittenKeyEvent;
      } else {
        return;
      }
    }

    // Ignore the release event if no key was pressed before. This is to
    // avoid the case when the user presses "enter" key to pop up the
    // shortcut input, release of the key is captured by accident.
    if (!this.pendingKeyEvent) {
      return;
    }

    if (this.updateOnKeyPress) {
      const updatedKeyEvent = {...keyEvent};
      const updatedPrerewrittenKeyEvent = {...prerewrittenKeyEvent};

      // If the key released is not a modifier, reset keyDisplay.
      if (!ModifierKeyCodes.includes(updatedKeyEvent.vkey as number)) {
        updatedKeyEvent.keyDisplay = '';
      }

      if (!ModifierKeyCodes.includes(
              updatedPrerewrittenKeyEvent.vkey as number)) {
        updatedPrerewrittenKeyEvent.keyDisplay = '';
      }

      // Update pending events with the modifications made to the key events
      // above.
      this.pendingKeyEvent = updatedKeyEvent;
      // console.log('pendingKeyEvent', pendingKeyEvent.keyDisplay);

      this.pendingPrerewrittenKeyEvent = updatedPrerewrittenKeyEvent;
    } else {
      // Only update the UI if the released key is the last key pressed OR if
      // its a modifier.
      if (this.pendingKeyEvent && keyEvent.vkey !== this.pendingKeyEvent.vkey) {
        if (!ModifierKeyCodes.includes(keyEvent.vkey as number)) {
          return;
        }

        this.pendingKeyEvent.modifiers = keyEvent.modifiers;
        this.pendingPrerewrittenKeyEvent!.modifiers =
            prerewrittenKeyEvent.modifiers;
        return;
      }

      this.pendingKeyEvent = keyEvent;
      this.pendingPrerewrittenKeyEvent = prerewrittenKeyEvent;

      this.dispatchEvent(new CustomEvent('shortcut-input-event', {
        bubbles: true,
        composed: true,
        detail: {
          keyEvent: this.pendingKeyEvent,
        },
      }));
    }
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
    this.eventTracker.add(this, 'blur', () => this.onBlur());
    this.$.container.focus();
    this.dispatchCaptureStateEvent();
  }

  onBlur(): void {
    if (this.ignoreBlur) {
      return;
    }

    this.stopObserving();
  }

  stopObserving(): void {
    this.isCapturing = false;
    this.shortcutInputProvider?.stopObservingShortcutInput();
    this.eventTracker.removeAll();
    this.dispatchCaptureStateEvent();
  }

  reset(): void {
    this.pendingKeyEvent = null;
    this.pendingPrerewrittenKeyEvent = null;
  }

  /**
   * Consumes all events received by the shortcut_input element.
   */
  private stopEvent(e: KeyboardEvent): void {
    e.preventDefault();
    e.stopPropagation();
  }


  private isModifier(keyEvent: KeyEvent): boolean {
    return ModifierKeyCodes.includes(keyEvent.vkey as number);
  }

  getKey(): string {
    const keyEvent = this.getPendingKeyEvent();
    if (keyEvent && keyEvent.keyDisplay != '' && !this.isModifier(keyEvent)) {
      const keyDisplay = keyEvent.keyDisplay;
      if (keyDisplay in KeyToIconNameMap) {
        return keyDisplay;
      }
      // <if expr="_google_chrome" >
      if (keyDisplay in KeyToInternalIconNameMap) {
        return keyDisplay;
      }
      // </if>
      return keyDisplay.toLowerCase();
    }
    return this.i18n('inputKeyPlaceholder');
  }

  getKeyState(): string {
    const keyEvent = this.getPendingKeyEvent();
    if (keyEvent && keyEvent.keyDisplay != '' && !this.isModifier(keyEvent)) {
      return KeyInputState.ALPHANUMERIC_SELECTED;
    }
    return KeyInputState.NOT_SELECTED;
  }

  getConfirmKey(): string {
    const keyEvent = this.getPendingKeyEvent();
    if (keyEvent && keyEvent.keyDisplay != '') {
      const keyDisplay = keyEvent.keyDisplay;
      if (keyDisplay in KeyToIconNameMap) {
        return keyDisplay;
      }
      // <if expr="_google_chrome" >
      if (keyDisplay in KeyToInternalIconNameMap) {
        return keyDisplay;
      }
      // </if>
      return keyDisplay.toLowerCase();
    }
    return this.i18n('inputKeyPlaceholder');
  }

  getConfirmKeyState(): string {
    const keyEvent = this.getPendingKeyEvent();
    if (keyEvent && keyEvent.keyDisplay != '' && this.isModifier(keyEvent)) {
      return KeyInputState.MODIFIER_SELECTED;
    }

    if (keyEvent && keyEvent.keyDisplay != '') {
      return KeyInputState.ALPHANUMERIC_SELECTED;
    }

    return KeyInputState.NOT_SELECTED;
  }

  shouldShowEditView(): boolean {
    return this.isCapturing || this.updateOnKeyPress;
  }

  shouldShowConfirmView(): boolean {
    return this.getPendingKeyEvent() !== null && !this.isCapturing &&
        !this.updateOnKeyPress;
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
  protected getFunctionState(): string {
    return this.getModifierState(Modifier.FN_KEY);
  }

  /**
   * Returns the specified CSS state of the modifier key element.
   */
  private getModifierState(modifier: Modifier): KeyInputState {
    const keyEvent = this.getPendingKeyEvent();
    if (keyEvent && keyEvent?.modifiers & modifier) {
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
      case Modifier.FN_KEY:
        return 'fn';
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
    return this.getPendingKeyEvent() !== null;
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

  private getPendingKeyEvent(): KeyEvent|null {
    return this.displayPrerewrittenKeyEvents ?
        this.pendingPrerewrittenKeyEvent :
        this.pendingKeyEvent;
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
