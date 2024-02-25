// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Helper functions to manage select UI elements.
 */

export interface SelectListTypeItem {
  optionGroupName?: string;
  selected?: boolean;
  title: string;
  value: string;
}
export type SelectListType = SelectListTypeItem[];

/**
 * Sets up given "select" element using the list and adds callback.
 * Creates option groups if needed.
 * @param select Select object to be updated.
 * @param list List of the options to be added. Elements with
 *     optionGroupName are considered option group.
 * @param callback Callback which should be called, or null
 *     if the event listener shouldn't be added.
 *
 * Note: do not forget to update getSelectedTitle() below if this is
 * updated!
 */
export function setupSelect(select: HTMLSelectElement, list: SelectListType,
    callback: ((arg0: string) => void)|null): void {
  select.innerHTML = window.trustedTypes ? window.trustedTypes.emptyHTML : '';
  let optgroup: HTMLOptGroupElement|null = null;
  for (const item of list) {
    if (item.optionGroupName) {
      optgroup = document.createElement('optgroup');
      optgroup.label = item.optionGroupName;
      select.appendChild(optgroup);
    } else {
      const option =
          new Option(item.title, item.value, item.selected, item.selected);
      if (optgroup instanceof HTMLOptGroupElement) {
        optgroup.appendChild(option);
      } else {
        select.appendChild(option);
      }
    }
  }
  if (callback) {
    const runCallback = function() {
      callback(select.options[select.selectedIndex].value);
    };
    select.addEventListener('input', runCallback);
  }
}

/**
 * Returns title of the selected option (see setupSelect() above).
 * @param list The same as in setupSelect() above.
 */
export function getSelectedTitle(list: SelectListType): string {
  let firstTitle = '';
  for (const item of list) {
    if (item.optionGroupName) {
      continue;
    }

    if (!firstTitle) {
      firstTitle = item.title;
    }

    if (item.selected) {
      return item.title;
    }
  }
  return firstTitle;
}

/**
 * Returns value of the selected option (see setupSelect() above).
 * @param list The same as in setupSelect() above.
 */
export function getSelectedValue(list: SelectListType): string|null {
  for (const item of list) {
    if (item.optionGroupName) {
      continue;
    }
    if (item.selected) {
      return item.value;
    }
  }
  return null;
}
