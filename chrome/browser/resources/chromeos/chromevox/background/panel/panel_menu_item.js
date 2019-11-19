// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview An item in a drop-down menu in the ChromeVox panel.
 */

goog.provide('PanelMenuItem');

goog.require('EventSourceType');

/**
 * @param {string} menuItemTitle The title of the menu item.
 * @param {string} menuItemShortcut The keystrokes to select this item.
 * @param {string} menuItemBraille The braille keystrokes to select this item.
 * @param {string} gesture The gesture to select this item.
 * @param {Function} callback The function to call if this item is selected.
 * @constructor
 */
PanelMenuItem = function(
    menuItemTitle, menuItemShortcut, menuItemBraille, gesture, callback) {
  this.callback = callback;

  this.element = document.createElement('tr');
  this.element.className = 'menu-item';
  this.element.tabIndex = -1;
  this.element.setAttribute('role', 'menuitem');

  this.element.addEventListener(
      'mouseover', (function(evt) {
                     this.element.focus();
                   }).bind(this),
      false);

  var title = document.createElement('td');
  title.className = 'menu-item-title';
  title.textContent = menuItemTitle;

  // Tooltip in case the menu item is cut off.
  title.title = menuItemTitle;
  this.element.appendChild(title);

  var backgroundWindow = chrome.extension.getBackgroundPage();
  if (backgroundWindow['EventSourceState']['get']() ==
      EventSourceType.TOUCH_GESTURE) {
    var gestureNode = document.createElement('td');
    gestureNode.className = 'menu-item-shortcut';
    gestureNode.textContent = gesture;
    this.element.appendChild(gestureNode);
    return;
  }

  var shortcut = document.createElement('td');
  shortcut.className = 'menu-item-shortcut';
  shortcut.textContent = menuItemShortcut;
  this.element.appendChild(shortcut);

  if (localStorage['brailleCaptions'] === String(true)) {
    var braille = document.createElement('td');
    braille.className = 'menu-item-shortcut';
    braille.textContent = menuItemBraille;
    this.element.appendChild(braille);
  }
};

PanelMenuItem.prototype = {
  get text() {
    return this.element.textContent;
  }
};
