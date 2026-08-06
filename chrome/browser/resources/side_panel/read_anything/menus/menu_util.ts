// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {SettingsOption, ShowAtConfigPrefs} from '../content/read_anything_types.js';

export enum SettingsItemType {
  MENU = 1,
  TOGGLE = 2,
  ACTION = 3,
  RADIO = 4,
  EXPAND = 5,
}

// Represents top-level items of the settings menu. Also used for toggle items
// in a dropdown menu.
export interface SettingsItem {
  id: SettingsOption;
  icon: string;
  title: string;
  itemType: SettingsItemType;
  // Whether the toggle is checked. Only used when itemType is TOGGLE
  checked?: boolean;
  // Whether the toggle is disabled. Only used when itemType is TOGGLE
  disabled?: boolean;
  // Needed when the aria label should be different from the title
  ariaLabel?: string;
  showSeparator?: boolean;
}

// Represents a single menu item in a dropown menu in the toolbar.
export interface MenuStateItem<T> {
  data: T;        // The value that is propagated when this item is selected.
  title: string;  // The visible text for this item.
  selected?: boolean;  // Whether this item is currently selected.
  icon?: string;   // An optional icon that is displayed next to the title.
  style?: string;  // An optional string for styling each item.
  // Needed when the aria label should be different from the title
  ariaLabel?: string;
  // Optional semantic item category. Defaults to SettingsItemType.RADIO if
  // omitted.
  itemType?: SettingsItemType;
}

export interface MenuHeader {
  title: string;
  separator: boolean;
  // Optional keyboard shortcut to display.
  shortcut?: string;
}

export interface MenuGroup<T> {
  header: MenuHeader;
  items: Array<MenuStateItem<T>>;
  eventName: string;
}

// Defines the contract for any menu that appears in the Toolbar.
// Ensures consistent behavior for opening and closing logic.
export interface ToolbarMenu {
  open(anchor: HTMLElement, showAtConfig?: ShowAtConfigPrefs): void;
  close(): void;
}

// TODO(crbug.com/346612365): Consider renaming this method to be more
// descriptive.
// Returns the index of the item in menuArray that contains the given data.
export function getIndexOfSetting(
    menuArray: Array<MenuStateItem<unknown>>, dataToFind: unknown): number {
  return menuArray.findIndex((item) => (item.data === dataToFind));
}

// Returns the index of the item in menuArray that contains the given data. If
// the given data does not exist in the menuArray anymore, returns the first
// index.
export function getIndexOrDefault(
    menuArray: Array<MenuStateItem<unknown>>, data: unknown): number {
  const index = getIndexOfSetting(menuArray, data);

  if (index < 0 && menuArray.length > 0) {
    return 0;
  }

  return index;
}
