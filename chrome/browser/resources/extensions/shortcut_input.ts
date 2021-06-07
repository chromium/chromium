// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_icons_css.m.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.m.js';
import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import 'chrome://resources/polymer/v3_0/paper-styles/color.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {IronA11yAnnouncer} from 'chrome://resources/polymer/v3_0/iron-a11y-announcer/iron-a11y-announcer.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {KeyboardShortcutDelegate} from './keyboard_shortcut_delegate.js';
import {hasValidModifiers, isValidKeyCode, Key, keystrokeToString} from './shortcut_util.js';

enum ShortcutError {
  NO_ERROR = 0,
  INCLUDE_START_MODIFIER = 1,
  TOO_MANY_MODIFIERS = 2,
  NEED_CHARACTER = 3,
}

// The UI to display and manage keyboard shortcuts set for extension commands.

interface ExtensionsShortcutInputElement {
  $: {
    input: HTMLElement,
    edit: HTMLElement,
  };
}

const ExtensionsShortcutInputElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement) as
    {new (): PolymerElement & I18nBehavior};

class ExtensionsShortcutInputElement extends
    ExtensionsShortcutInputElementBase {
  static get is() {
    return 'extensions-shortcut-input';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      delegate: Object,

      item: {
        type: String,
        value: '',
      },

      commandName: {
        type: String,
        value: '',
      },

      shortcut: {
        type: String,
        value: '',
      },

      capturing_: {
        type: Boolean,
        value: false,
      },

      error_: {
        type: Number,
        value: ShortcutError.NO_ERROR,
      },


      readonly_: {
        type: Boolean,
        value: true,
        reflectToAttribute: true,
      },

      pendingShortcut_: {
        type: String,
        value: '',
      },
    };
  }

  delegate: KeyboardShortcutDelegate;
  item: string;
  commandName: string;
  shortcut: string;
  private capturing_: boolean;
  private error_: ShortcutError;
  private readonly_: boolean;
  private pendingShortcut_: string;

  ready() {
    super.ready();

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

    if (e.keyCode === Key.Escape) {
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
    if (e.keyCode === Key.Tab) {
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

    if (e.keyCode === Key.Escape || e.keyCode === Key.Tab) {
      return;
    }

    this.handleKey_(e);
  }

  private getErrorString_(
      _error: ShortcutError, includeStartModifier: string,
      tooManyModifiers: string, needCharacter: string): string {
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

    IronA11yAnnouncer.requestAvailability();
    this.dispatchEvent(new CustomEvent('iron-announce', {
      bubbles: true,
      composed: true,
      detail: {
        text: this.i18n('shortcutSet', this.computeText_()),
      }
    }));

    this.commitPending_();
    this.endCapture_();
  }

  private commitPending_() {
    this.shortcut = this.pendingShortcut_;
    this.delegate.updateExtensionCommandKeybinding(
        this.item, this.commandName, this.shortcut);
  }

  private computePlaceholder_(): string {
    if (this.readonly_) {
      return this.shortcut ? this.i18n('shortcutSet', this.computeText_()) :
                             this.i18n('shortcutNotSet');
    }
    return this.i18n('shortcutTypeAShortcut');
  }

  /**
   * @return The text to be displayed in the shortcut field.
   */
  private computeText_(): string {
    const shortcutString =
        this.capturing_ ? this.pendingShortcut_ : this.shortcut;
    return shortcutString.split('+').join(' + ');
  }

  private getIsInvalid_(): boolean {
    return this.error_ !== ShortcutError.NO_ERROR;
  }

  private onEditClick_() {
    // TODO(ghazale): The clearing functionality should be improved.
    // Instead of clicking the edit button, and then clicking elsewhere to
    // commit the "empty" shortcut, we want to introduce a separate clear
    // button.
    this.clearShortcut_();
    this.readonly_ = false;
    this.$.input.focus();
  }
}

customElements.define(
    ExtensionsShortcutInputElement.is, ExtensionsShortcutInputElement);
