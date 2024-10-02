// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';

import {getInstance as getAnnouncerInstance} from 'chrome://resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import type {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {createDummyExtensionInfo} from './item_util.js';
import type {KeyboardShortcutDelegate} from './keyboard_shortcut_delegate.js';
import {createDummyKeyboardShortcutDelegate} from './keyboard_shortcut_delegate.js';
import {getCss} from './shortcut_input.css.js';
import {getHtml} from './shortcut_input.html.js';
import {formatShortcutText, hasValidModifiers, isValidKeyCode, Key, keystrokeToString} from './shortcut_util.js';

enum ShortcutError {
  NO_ERROR = 0,
  INCLUDE_START_MODIFIER = 1,
  TOO_MANY_MODIFIERS = 2,
  NEED_CHARACTER = 3,
}

// The UI to display and manage keyboard shortcuts set for extension commands.

export interface ExtensionsShortcutInputElement {
  $: {
    input: CrInputElement,
    edit: HTMLElement,
  };
}

const ExtensionsShortcutInputElementBase = I18nMixinLit(CrLitElement);

export class ExtensionsShortcutInputElement extends
    ExtensionsShortcutInputElementBase {
  static get is() {
    return 'extensions-shortcut-input';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      delegate: {type: Object},
      item: {type: Object},
      command: {type: Object},
      shortcut: {type: String},
      error_: {type: Number},

      readonly_: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  delegate: KeyboardShortcutDelegate = createDummyKeyboardShortcutDelegate();
  item: chrome.developerPrivate.ExtensionInfo = createDummyExtensionInfo();
  command: chrome.developerPrivate.Command = {
    description: '',
    keybinding: '',
    name: '',
    isActive: false,
    scope: chrome.developerPrivate.CommandScope.CHROME,
    isExtensionAction: false,
  };
  shortcut: string = '';
  protected readonly_: boolean = true;
  private capturing_: boolean = false;
  private error_: ShortcutError = ShortcutError.NO_ERROR;
  private pendingShortcut_: string = '';

  override firstUpdated() {
    const node = this.$.input;
    node.addEventListener('mouseup', this.startCapture_.bind(this));
    node.addEventListener('blur', this.endCapture_.bind(this));
    node.addEventListener('focus', this.startCapture_.bind(this));
    node.addEventListener('keydown', this.onKeyDown_.bind(this));
    node.addEventListener('keyup', this.onKeyUp_.bind(this));
  }

  private startCapture_() {
    if (this.capturing_ || this.readonly_) {
      return;
    }
    this.capturing_ = true;
    this.delegate.setShortcutHandlingSuspended(true);
  }

  private endCapture_() {
    if (!this.capturing_) {
      return;
    }
    this.pendingShortcut_ = '';
    this.capturing_ = false;
    this.$.input.blur();
    this.error_ = ShortcutError.NO_ERROR;
    this.delegate.setShortcutHandlingSuspended(false);
    this.readonly_ = true;
  }

  private clearShortcut_() {
    this.pendingShortcut_ = '';
    this.shortcut = '';
    // We commit the empty shortcut in order to clear the current shortcut
    // for the extension.
    this.commitPending_();
    this.endCapture_();
  }

  private onKeyDown_(e: KeyboardEvent) {
    if (this.readonly_) {
      return;
    }

    if (e.target === this.$.edit) {
      return;
    }

    if (e.keyCode === Key.ESCAPE) {
      if (!this.capturing_) {
        // If we're not currently capturing, allow escape to propagate.
        return;
      }
      // Otherwise, escape cancels capturing.
      this.endCapture_();
      e.preventDefault();
      e.stopPropagation();
      return;
    }
    if (e.keyCode === Key.TAB) {
      // Allow tab propagation for keyboard navigation.
      return;
    }

    if (!this.capturing_) {
      this.startCapture_();
    }

    this.handleKey_(e);
  }

  private onKeyUp_(e: KeyboardEvent) {
    // Ignores pressing 'Space' or 'Enter' on the edit button. In 'Enter's
    // case, the edit button disappears before key-up, so 'Enter's key-up
    // target becomes the input field, not the edit button, and needs to
    // be caught explicitly.
    if (this.readonly_) {
      return;
    }

    if (e.target === this.$.edit || e.key === 'Enter') {
      return;
    }

    if (e.keyCode === Key.ESCAPE || e.keyCode === Key.TAB) {
      return;
    }

    this.handleKey_(e);
  }

  protected getErrorString_(
      includeStartModifier: string, tooManyModifiers: string,
      needCharacter: string): string {
    switch (this.error_) {
      case ShortcutError.INCLUDE_START_MODIFIER:
        return includeStartModifier;
      case ShortcutError.TOO_MANY_MODIFIERS:
        return tooManyModifiers;
      case ShortcutError.NEED_CHARACTER:
        return needCharacter;
      default:
        assert(this.error_ === ShortcutError.NO_ERROR);
        return '';
    }
  }

  private handleKey_(e: KeyboardEvent) {
    // While capturing, we prevent all events from bubbling, to prevent
    // shortcuts lacking the right modifier (F3 for example) from activating
    // and ending capture prematurely.
    e.preventDefault();
    e.stopPropagation();

    // We don't allow both Ctrl and Alt in the same keybinding.
    // TODO(devlin): This really should go in hasValidModifiers,
    // but that requires updating the existing page as well.
    if (e.ctrlKey && e.altKey) {
      this.error_ = ShortcutError.TOO_MANY_MODIFIERS;
      return;
    }
    if (!hasValidModifiers(e)) {
      this.pendingShortcut_ = '';
      this.error_ = ShortcutError.INCLUDE_START_MODIFIER;
      return;
    }
    this.pendingShortcut_ = keystrokeToString(e);
    if (!isValidKeyCode(e.keyCode)) {
      this.error_ = ShortcutError.NEED_CHARACTER;
      return;
    }

    this.error_ = ShortcutError.NO_ERROR;

    getAnnouncerInstance().announce(
        this.i18n('shortcutSet', formatShortcutText(this.pendingShortcut_)));

    this.commitPending_();
    this.endCapture_();
  }

  private commitPending_() {
    this.shortcut = this.pendingShortcut_;
    this.delegate.updateExtensionCommandKeybinding(
        this.item.id, this.command.name, this.shortcut);
  }

  protected computeInputAriaLabel_(): string {
    return this.i18n(
        'editShortcutInputLabel', this.command.description, this.item.name);
  }

  protected computeEditButtonAriaLabel_(): string {
    return this.i18n(
        'editShortcutButtonLabel', this.command.description, this.item.name);
  }

  protected computePlaceholder_(): string {
    if (this.readonly_) {
      return this.shortcut ? this.i18n('shortcutSet', this.computeText_()) :
                             this.i18n('shortcutNotSet');
    }
    return this.i18n('shortcutTypeAShortcut');
  }

  /**
   * @return The text to be displayed in the shortcut field.
   */
  protected computeText_(): string {
    return formatShortcutText(this.shortcut);
  }

  protected getIsInvalid_(): boolean {
    return this.error_ !== ShortcutError.NO_ERROR;
  }

  protected onEditClick_() {
    // TODO(ghazale): The clearing functionality should be improved.
    // Instead of clicking the edit button, and then clicking elsewhere to
    // commit the "empty" shortcut, we want to introduce a separate clear
    // button.
    this.clearShortcut_();
    this.readonly_ = false;
    this.$.input.focus();
  }
}

// Exported to be used in the autogenerated Lit template file
export type ShortcutInputElement = ExtensionsShortcutInputElement;

declare global {
  interface HTMLElementTagNameMap {
    'extensions-shortcut-input': ExtensionsShortcutInputElement;
  }
}

customElements.define(
    ExtensionsShortcutInputElement.is, ExtensionsShortcutInputElement);
