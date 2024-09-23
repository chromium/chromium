// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Represents a single menu item in a dropown menu in the toolbar.
export interface MenuStateItem<T> {
  data: T;        // The value that is propagated when this item is selected.
  title: string;  // The visible text for this item.
  icon?: string;  // An optional icon that is displayed next to the title.
}

// TODO(crbug.com/346612365): Consider renaming this method to be more
// descriptive.
// Returns the index of the item in menuArray that contains the given data.
export function getIndexOfSetting(
    menuArray: Array<MenuStateItem<any>>, dataToFind: any): number {
  return menuArray.findIndex((item) => (item.data === dataToFind));
}
