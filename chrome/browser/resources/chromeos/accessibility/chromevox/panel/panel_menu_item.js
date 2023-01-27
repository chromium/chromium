// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview An item in a drop-down menu in the ChromeVox panel.
 */
import {LocalStorage} from '../../common/local_storage.js';
import {BackgroundBridge} from '../common/background_bridge.js';
import {EventSourceType} from '../common/event_source_type.js';
import {SettingsManager} from '../common/settings_manager.js';

export class PanelMenuItem {
  /**
   * @param {string} menuItemTitle The title of the menu item.
   * @param {string} menuItemShortcut The keystrokes to select this item.
   * @param {string} menuItemBraille The braille keystrokes to select this item.
   * @param {string} gesture The gesture to select this item.
   * @param {function() : !Promise} callback The function to call if this item
   *     is selected.
   * @param {string=} opt_id An optional id for the menu item element.
   */
  constructor(
      menuItemTitle, menuItemShortcut, menuItemBraille, gesture, callback,
      opt_id) {
    /** @type {string} */
    this.menuItemTitle = menuItemTitle;
    /** @type {string} */
    this.menuItemShortcut = menuItemShortcut;
    /** @type {string} */
    this.menuItemBraille = menuItemBraille;
    /** @type {string} */
    this.gesture = gesture;
    /** @type {function() : !Promise} */
    this.callback = callback;

    /** @type {Element} */
    this.element;
    /** @type {boolean} */
    this.enabled_ = true;

    this.init_(opt_id);
  }

  /**
   * @param {string=} opt_id
   * @private
   */
  async init_(opt_id) {
    this.element = document.createElement('tr');
    this.element.className = 'menu-item';
    this.element.tabIndex = -1;
    this.element.setAttribute('role', 'menuitem');
    if (opt_id) {
      this.element.id = opt_id;
    }

    this.element.addEventListener(
        'mouseover', () => this.element.focus(), false);

    const title = document.createElement('td');
    title.className = 'menu-item-title';
    title.textContent = this.menuItemTitle;

    // Tooltip in case the menu item is cut off.
    title.title = this.menuItemTitle;
    this.element.appendChild(title);

    const eventSource = await BackgroundBridge.EventSource.get();
    if (eventSource === EventSourceType.TOUCH_GESTURE) {
      const gestureNode = document.createElement('td');
      gestureNode.className = 'menu-item-shortcut';
      gestureNode.textContent = this.gesture;
      this.element.appendChild(gestureNode);
      return;
    }

    const shortcut = document.createElement('td');
    shortcut.className = 'menu-item-shortcut';
    shortcut.textContent = this.menuItemShortcut;
    this.element.appendChild(shortcut);

    if (LocalStorage.get('brailleCaptions') ||
        SettingsManager.get('menuBrailleCommands')) {
      const braille = document.createElement('td');
      braille.className = 'menu-item-shortcut';
      braille.textContent = this.menuItemBraille;
      this.element.appendChild(braille);
    }
  }

  /**
   * @return {string} The text content of this menu item.
   */
  get text() {
    return this.element.textContent;
  }

  /**
   * @return {boolean} The enabled state of this item.
   */
  get enabled() {
    return this.enabled_;
  }

  /**
   * Marks this item as disabled.
   */
  disable() {
    this.enabled_ = false;
    this.element.classList.add('disabled');
    this.element.setAttribute('aria-disabled', true);
  }
}
