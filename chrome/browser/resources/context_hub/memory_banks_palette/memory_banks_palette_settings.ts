// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/icons.html.js';

import {getInstance as getAnnouncerInstance} from '//resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import {assertNotReachedCase} from '//resources/js/assert.js';
import {isMac} from '//resources/js/platform.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {browserProxyFactory, MemoryBanksShortcutStatus} from '../context_hub.mojom-webui.js';

import {getCss} from './memory_banks_palette_settings.css.js';
import {getHtml} from './memory_banks_palette_settings.html.js';

/** `KeyboardEvent.key` values of modifier keys pressed by themselves. */
const MODIFIER_KEYS = new Set(['Shift', 'Control', 'Alt', 'AltGraph', 'Meta']);

/**
 * Converts a key press into the ui::Command accelerator string format used by
 * the browser (for example, "Ctrl+Shift+K" or "Command+Shift+K"). Returns an
 * empty string unless the key is a letter or a number pressed together with
 * Ctrl, Alt, or (on macOS) Command, matching what the browser accepts. The
 * Windows key and the ChromeOS Search key are reserved by the operating system.
 */
export function keyboardEventToShortcut(e: KeyboardEvent): string {
  const isLetter =
      e.keyCode >= 'A'.charCodeAt(0) && e.keyCode <= 'Z'.charCodeAt(0);
  const isDigit =
      e.keyCode >= '0'.charCodeAt(0) && e.keyCode <= '9'.charCodeAt(0);
  if ((!isLetter && !isDigit) || (e.metaKey && !isMac)) {
    return '';
  }
  const tokens: string[] = [];
  if (e.metaKey) {
    tokens.push('Command');
  }
  if (e.ctrlKey) {
    tokens.push('Ctrl');
  }
  if (e.altKey) {
    tokens.push('Alt');
  }
  if (tokens.length === 0) {
    return '';
  }
  if (e.shiftKey) {
    tokens.push('Shift');
  }
  tokens.push(String.fromCharCode(e.keyCode));
  return tokens.join('+');
}

/** Formats the modifier keys currently held down, for a live preview. */
function formatHeldModifiers(e: KeyboardEvent): string {
  const tokens: string[] = [];
  if (isMac) {
    if (e.ctrlKey) {
      tokens.push('⌃');
    }
    if (e.altKey) {
      tokens.push('⌥');
    }
    if (e.shiftKey) {
      tokens.push('⇧');
    }
    if (e.metaKey) {
      tokens.push('⌘');
    }
    return tokens.join('');
  }
  if (e.ctrlKey) {
    tokens.push('Ctrl');
  }
  if (e.altKey) {
    tokens.push('Alt');
  }
  if (e.shiftKey) {
    tokens.push('Shift');
  }
  return tokens.length > 0 ? `${tokens.join('+')}+` : '';
}

/**
 * Settings panel of the Memory Banks palette. Lets the user set, change, or
 * remove the keyboard shortcut that opens the palette, and open Context Hub.
 *
 * Fires:
 *   - 'back-click' when the user leaves the settings panel.
 *   - 'shortcut-changed' with the new shortcut display text ('' if removed).
 */
export interface MemoryBanksPaletteSettingsElement {
  $: {
    backButton: HTMLElement,
    openContextHubButton: HTMLButtonElement,
    shortcutField: HTMLElement,
  };
}

export class MemoryBanksPaletteSettingsElement extends CrLitElement {
  static get is() {
    return 'memory-banks-palette-settings';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      currentShortcutDisplayText: {type: String},
      suggestedShortcutAccelerator: {type: String},
      suggestedShortcutDisplayText: {type: String},
      isRecording: {type: Boolean, state: true},
      preview: {type: String, state: true},
      error: {type: String, state: true},
    };
  }

  accessor currentShortcutDisplayText: string = '';
  accessor suggestedShortcutAccelerator: string = '';
  accessor suggestedShortcutDisplayText: string = '';
  accessor isRecording: boolean = false;
  /** Live preview of the modifier keys held while recording. */
  protected accessor preview: string = '';
  protected accessor error: string = '';

  private isSaving: boolean = false;

  /** Starts recording a new shortcut and focuses the shortcut field. */
  async startRecording() {
    this.isRecording = true;
    this.preview = '';
    this.error = '';
    await this.updateComplete;
    this.$.shortcutField.focus();
  }

  /** Moves focus to the back button, e.g. when the panel opens. */
  async focusBackButton() {
    await this.updateComplete;
    this.$.backButton.focus();
  }

  protected onBackButtonClick() {
    this.stopRecording();
    this.fire('back-click');
  }

  protected async onOpenContextHubButtonClick() {
    await browserProxyFactory.getInstance().handler.openContextHubPage();
    window.close();
  }

  protected async onEditButtonClick() {
    await this.startRecording();
  }

  protected async onUseSuggestedButtonClick() {
    await this.saveShortcut(this.suggestedShortcutAccelerator);
  }

  protected shouldShowSuggestion(): boolean {
    return !this.currentShortcutDisplayText && !this.isRecording &&
        !!this.suggestedShortcutAccelerator &&
        !!this.suggestedShortcutDisplayText;
  }

  protected async onRemoveButtonClick() {
    await this.saveShortcut('');
  }

  protected async onShortcutFieldClick() {
    if (!this.isRecording) {
      await this.startRecording();
    }
  }

  protected onShortcutFieldBlur() {
    this.stopRecording();
  }

  protected async onShortcutFieldKeydown(e: KeyboardEvent) {
    if (!this.isRecording ||
        (e.key === 'Tab' && !e.ctrlKey && !e.altKey && !e.metaKey)) {
      return;
    }
    // While recording, keep key presses from triggering anything else.
    e.preventDefault();
    e.stopPropagation();
    // Ignore keys held down and keys used to compose characters.
    if (e.repeat || e.isComposing) {
      return;
    }

    if (e.key === 'Escape') {
      this.stopRecording();
      await this.focusEditButton();
      return;
    }
    if (MODIFIER_KEYS.has(e.key)) {
      // Starting a new combination clears the previous error.
      this.error = '';
      this.preview = formatHeldModifiers(e);
      return;
    }
    if (e.metaKey && !isMac) {
      this.showError(
          'The Windows and Search keys are reserved. Use Ctrl or Alt instead.');
      return;
    }
    if (e.metaKey && e.ctrlKey && e.altKey && e.shiftKey) {
      this.showError('Use up to three modifier keys.');
      return;
    }

    const shortcut = keyboardEventToShortcut(e);
    if (!shortcut) {
      this.showError(this.getMissingModifierError());
      return;
    }
    await this.saveShortcut(shortcut);
  }

  protected onShortcutFieldKeyup(e: KeyboardEvent) {
    if (this.isRecording && !this.error && MODIFIER_KEYS.has(e.key)) {
      this.preview = formatHeldModifiers(e);
    }
  }

  protected getFieldText(): string {
    if (this.isRecording) {
      return this.preview || 'Press the keys you want to use';
    }
    return this.currentShortcutDisplayText || 'Not set';
  }

  protected getFieldClass(): string {
    const classes: string[] = [];
    if (this.isRecording) {
      classes.push('recording');
    } else if (!this.currentShortcutDisplayText) {
      classes.push('placeholder');
    }
    if (this.error) {
      classes.push('invalid');
    }
    return classes.join(' ');
  }

  protected getModifierHint(): string {
    return isMac ? '⌘ or Control' : 'Ctrl or Alt';
  }

  protected getFieldDescribedBy(): string {
    return this.error ? 'shortcutDescription error' : 'shortcutDescription';
  }

  protected getEditButtonAriaLabel(): string {
    return this.currentShortcutDisplayText ?
        `Change keyboard shortcut, currently ${
            this.currentShortcutDisplayText}` :
        'Set keyboard shortcut';
  }

  private stopRecording() {
    this.isRecording = false;
    this.preview = '';
    this.error = '';
  }

  private async focusEditButton() {
    await this.updateComplete;
    this.shadowRoot.querySelector<HTMLElement>('#editButton')?.focus();
  }

  private async saveShortcut(shortcut: string) {
    if (this.isSaving) {
      return;
    }
    this.isSaving = true;
    // Clearing the error lets screen readers announce it again on a retry.
    this.error = '';
    const startedWhileRecording = this.isRecording;
    try {
      const handler = browserProxyFactory.getInstance().handler;
      const {status} = await handler.setBrowseMemoryBanksShortcut(shortcut);
      // Don't show a stale error if recording was cancelled or the panel was
      // closed while the browser checked the shortcut.
      const recordingCancelled = startedWhileRecording && !this.isRecording;
      if (status !== MemoryBanksShortcutStatus.kValid &&
          (recordingCancelled || this.hidden)) {
        return;
      }
      switch (status) {
        case MemoryBanksShortcutStatus.kValid: {
          const {displayText} = await handler.getBrowseMemoryBanksShortcut();
          this.currentShortcutDisplayText = displayText;
          this.stopRecording();
          this.fire('shortcut-changed', displayText);
          getAnnouncerInstance().announce(
              displayText ? `Shortcut set to ${displayText}` :
                            'Shortcut removed');
          await this.focusEditButton();
          return;
        }
        case MemoryBanksShortcutStatus.kInvalid:
          this.showError(this.getMissingModifierError());
          return;
        case MemoryBanksShortcutStatus.kDisallowedModifiers:
          this.showError(
              isMac ?
                  'Include ⌘ or Control. Option by itself is used to type ' +
                      'special characters.' :
                  'Ctrl+Alt can\'t be used because some keyboards need it ' +
                      'to type characters. Try Alt+Shift or Ctrl+Shift ' +
                      'instead.');
          return;
        case MemoryBanksShortcutStatus.kConflictsWithBrowserShortcut:
          this.showError(
              'Chrome already uses this shortcut. Try a different one.');
          return;
        default:
          assertNotReachedCase(status);
      }
    } finally {
      this.isSaving = false;
    }
  }

  private showError(message: string) {
    this.preview = '';
    this.error = message;
  }

  private getMissingModifierError(): string {
    return `Use a letter or number together with ${this.getModifierHint()}.`;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'memory-banks-palette-settings': MemoryBanksPaletteSettingsElement;
  }
}

customElements.define(
    MemoryBanksPaletteSettingsElement.is, MemoryBanksPaletteSettingsElement);
