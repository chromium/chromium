// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/cr_elements/md_select_css.m.js';
import 'chrome://resources/polymer/v3_0/paper-styles/color.js';
import './shortcut_input.js';

import {CrContainerShadowBehavior} from 'chrome://resources/cr_elements/cr_container_shadow_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {KeyboardShortcutDelegate} from './keyboard_shortcut_delegate.js';

/** Event interface for dom-repeat. */
interface RepeaterEvent<T> extends CustomEvent {
  model: {
    get: (name: string) => T,
    set: (name: string, val: T) => void,
    index: number,
  };
}

const ExtensionsKeyboardShortcutsElementBase =
    mixinBehaviors([CrContainerShadowBehavior], PolymerElement) as
    {new (): PolymerElement};

// The UI to display and manage keyboard shortcuts set for extension commands.
class ExtensionsKeyboardShortcutsElement extends
    ExtensionsKeyboardShortcutsElementBase {
  static get is() {
    return 'extensions-keyboard-shortcuts';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      delegate: Object,

      items: Array,

      /**
       * Proxying the enum to be used easily by the html template.
       */
      CommandScope_: {
        type: Object,
        value: chrome.developerPrivate.CommandScope,
      },
    };
  }

  delegate: KeyboardShortcutDelegate;
  items: Array<chrome.developerPrivate.ExtensionInfo>;

  ready() {
    super.ready();
    this.addEventListener('view-enter-start', this.onViewEnter_);
  }

  private onViewEnter_() {
    chrome.metricsPrivate.recordUserAction('Options_ExtensionCommands');
  }

  private calculateShownItems_(): Array<chrome.developerPrivate.ExtensionInfo> {
    return this.items.filter(function(item) {
      return item.commands.length > 0;
    });
  }

  /**
   * A polymer bug doesn't allow for databinding of a string property as a
   * boolean, but it is correctly interpreted from a function.
   * Bug: https://github.com/Polymer/polymer/issues/3669
   */
  private hasKeybinding_(keybinding: string): boolean {
    return !!keybinding;
  }

  /**
   * Determines whether to disable the dropdown menu for the command's scope.
   */
  private computeScopeDisabled_(command: chrome.developerPrivate.Command):
      boolean {
    return command.isExtensionAction || !command.isActive;
  }

  /**
   * This function exists to force trigger an update when CommandScope_
   * becomes available.
   */
  private triggerScopeChange_(scope: chrome.developerPrivate.CommandScope) {
    return scope;
  }

  private onCloseButtonClick_() {
    this.dispatchEvent(
        new CustomEvent('close', {bubbles: true, composed: true}));
  }

  private onScopeChanged_(event: RepeaterEvent<string>) {
    this.delegate.updateExtensionCommandScope(
        event.model.get('item.id'), event.model.get('command.name'),
        ((event.target as HTMLSelectElement).value as
         chrome.developerPrivate.CommandScope));
  }
}

customElements.define(
    ExtensionsKeyboardShortcutsElement.is, ExtensionsKeyboardShortcutsElement);
