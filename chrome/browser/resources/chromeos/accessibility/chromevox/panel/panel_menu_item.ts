// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview An item in a drop-down menu in the ChromeVox panel.
 */
import {LocalStorage} from '/common/local_storage.js';

import {BackgroundBridge} from '../common/background_bridge.js';
import {EventSourceType} from '../common/event_source_type.js';
import {SettingsManager} from '../common/settings_manager.js';

export class PanelMenuItem {
  callback: () => Promise<void>;
  element?: HTMLElement;
  gesture?: string;
  menuItemBraille?: string;
  menuItemShortcut?: string;
  menuItemTitle: string;

  private enabled_ = true;

  /**
   * @param menuItemTitle The title of the menu item.
   * @param menuItemShortcut The keystrokes to select this item.
   * @param menuItemBraille The braille keystrokes to select this item.
   * @param gesture The gesture to select this item.
   * @param callback The function to call if this item is selected.
   * @param optId An optional id for the menu item element.
   */
  constructor(
      menuItemTitle: string, menuItemShortcut: string | undefined,
      menuItemBraille: string | undefined, gesture: string | undefined,
      callback: () => Promise<void>, optId?: string) {
    this.menuItemTitle = menuItemTitle;
    this.menuItemShortcut = menuItemShortcut;
    this.menuItemBraille = menuItemBraille;
    this.gesture = gesture;
    this.callback = callback;

    this.init_(optId);
  }

  private async init_(optId?: string): Promise<void> {
    this.element = document.createElement('tr');
    this.element.className = 'menu-item';
    this.element.tabIndex = -1;
    this.element.setAttribute('role', 'menuitem');
    if (optId) {
      this.element.id = optId;
    }

    // TODO(b/314203187): Not null asserted, check that this is correct.
    this.element.addEventListener(
        'mouseover', () => this.element!.focus(), false);

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
      gestureNode.textContent = this.gesture ?? null;
      this.element.appendChild(gestureNode);
      return;
    }

    const shortcut = document.createElement('td');
    shortcut.className = 'menu-item-shortcut';
    shortcut.textContent = this.menuItemShortcut ?? null;
    this.element.appendChild(shortcut);

    if (LocalStorage.get('brailleCaptions') ||
        SettingsManager.get('menuBrailleCommands')) {
      const braille = document.createElement('td');
      braille.className = 'menu-item-shortcut';
      braille.textContent = this.menuItemBraille ?? null;
      this.element.appendChild(braille);
    }
  }

  /** @return The text content of this menu item. */
  get text(): string {
    // TODO(b/314203187): Not null asserted, check that this is correct.
    return this.element!.textContent!;
  }

  /** @return The enabled state of this item. */
  get enabled(): boolean {
    return this.enabled_;
  }

  /** Marks this item as disabled. */
  disable(): void {
    this.enabled_ = false;
    // TODO(b/314203187): Not null asserted, check that this is correct.
    this.element!.classList.add('disabled');
    this.element!.setAttribute('aria-disabled', String(true));
  }
}
