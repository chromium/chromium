// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Represents a single menu item in a dropown menu in the toolbar.
export interface MenuStateItem<T> {
  data: T;        // The value that is propagated when this item is selected.
  title: string;  // The visible text for this item.
  icon?: string;  // An optional icon that is displayed next to the title.
  style?: string;  // An optional string for styling each item.
}

// TODO(crbug.com/346612365): Consider renaming this method to be more
// descriptive.
// Returns the index of the item in menuArray that contains the given data.
export function getIndexOfSetting(
    menuArray: Array<MenuStateItem<any>>, dataToFind: any): number {
  return menuArray.findIndex((item) => (item.data === dataToFind));
}

// Returns the index of the item in menuArray that contains the given data. If
// the given data does not exist in the menuArray anymore, returns the first
// index.
export function getIndexOrDefault(
    menuArray: Array<MenuStateItem<any>>, data: any): number {
  const index = getIndexOfSetting(menuArray, data);

  if (index < 0 && menuArray.length > 0) {
    return 0;
  }

  return index;
}
