// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/cr_elements/md_select_css.m.js';
import 'chrome://resources/polymer/v3_0/paper-styles/color.js';
import './shortcut_input.js';

import {CrContainerShadowBehavior} from 'chrome://resources/cr_elements/cr_container_shadow_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ItemBehavior} from './item_behavior.js';
import {KeyboardShortcutDelegate} from './keyboard_shortcut_delegate.js';

// The UI to display and manage keyboard shortcuts set for extension commands.
Polymer({
  is: 'extensions-keyboard-shortcuts',

  _template: html`{__html_template__}`,

  behaviors: [CrContainerShadowBehavior, ItemBehavior],

  properties: {
    /** @type {!KeyboardShortcutDelegate} */
    delegate: Object,

    /** @type {Array<!chrome.developerPrivate.ExtensionInfo>} */
    items: Array,

    /**
     * Proxying the enum to be used easily by the html template.
     * @private
     */
    CommandScope_: {
      type: Object,
      value: chrome.developerPrivate.CommandScope,
    },
  },

  listeners: {
    'view-enter-start': 'onViewEnter_',
  },

  /** @private */
  onViewEnter_: function() {
    chrome.metricsPrivate.recordUserAction('Options_ExtensionCommands');
  },

  /**
   * @return {!Array<!chrome.developerPrivate.ExtensionInfo>}
   * @private
   */
  calculateShownItems_: function() {
    return this.items.filter(function(item) {
      return item.commands.length > 0;
    });
  },

  /**
   * A polymer bug doesn't allow for databinding of a string property as a
   * boolean, but it is correctly interpreted from a function.
   * Bug: https://github.com/Polymer/polymer/issues/3669
   * @param {string} keybinding
   * @return {boolean}
   * @private
   */
  hasKeybinding_: function(keybinding) {
    return !!keybinding;
  },

  /**
   * Determines whether to disable the dropdown menu for the command's scope.
   * @param {!chrome.developerPrivate.Command} command
   * @return {boolean}
   * @private
   */
  computeScopeDisabled_: function(command) {
    return command.isExtensionAction || !command.isActive;
  },

  /**
   * This function exists to force trigger an update when CommandScope_
   * becomes available.
   * @param {string} scope
   * @return {string}
   */
  triggerScopeChange_: function(scope) {
    return scope;
  },

  /** @private */
  onCloseButtonClick_: function() {
    this.fire('close');
  },

  /**
   * @param {!{target: HTMLSelectElement, model: Object}} event
   * @private
   */
  onScopeChanged_: function(event) {
    this.delegate.updateExtensionCommandScope(
        event.model.get('item.id'), event.model.get('command.name'),
        /** @type {chrome.developerPrivate.CommandScope} */
        (event.target.value));
  },
});
