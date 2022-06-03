// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview An item in a drop-down menu in the ChromeVox panel.
 */

goog.provide('PanelMenuItem');

goog.require('EventSourceType');

PanelMenuItem = class {
  /**
   * @param {string} menuItemTitle The title of the menu item.
   * @param {string} menuItemShortcut The keystrokes to select this item.
   * @param {string} menuItemBraille The braille keystrokes to select this item.
   * @param {string} gesture The gesture to select this item.
   * @param {Function} callback The function to call if this item is selected.
   * @param {string=} opt_id An optional id for the menu item element.
   */
  constructor(
      menuItemTitle, menuItemShortcut, menuItemBraille, gesture, callback,
      opt_id) {
    // Save inputs.
    this.menuItemTitle = menuItemTitle;
    this.menuItemShortcut = menuItemShortcut;
    this.menuItemBraille = menuItemBraille;
    this.gesture = gesture;
    this.callback = callback;

    /** @type {boolean} */
    this.enabled_ = true;

    this.element = document.createElement('tr');
    this.element.className = 'menu-item';
    this.element.tabIndex = -1;
    this.element.setAttribute('role', 'menuitem');
    if (opt_id) {
      this.element.id = opt_id;
    }

    this.element.addEventListener(
        'mouseover', (function(evt) {
                       this.element.focus();
                     }).bind(this),
        false);

    const title = document.createElement('td');
    title.className = 'menu-item-title';
    title.textContent = menuItemTitle;

    // Tooltip in case the menu item is cut off.
    title.title = menuItemTitle;
    this.element.appendChild(title);

    const backgroundWindow = chrome.extension.getBackgroundPage();
    if (backgroundWindow['EventSourceState']['get']() ===
        EventSourceType.TOUCH_GESTURE) {
      const gestureNode = document.createElement('td');
      gestureNode.className = 'menu-item-shortcut';
      gestureNode.textContent = gesture;
      this.element.appendChild(gestureNode);
      return;
    }

    const shortcut = document.createElement('td');
    shortcut.className = 'menu-item-shortcut';
    shortcut.textContent = menuItemShortcut;
    this.element.appendChild(shortcut);

    if (localStorage['brailleCaptions'] === String(true) ||
        localStorage['menuBrailleCommands'] === String(true)) {
      const braille = document.createElement('td');
      braille.className = 'menu-item-shortcut';
      braille.textContent = menuItemBraille;
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
};
