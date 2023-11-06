// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Helper functions to manage select UI elements.
 */

/**
 * @typedef {
 *   Iterable<{
 *       optionGroupName: (string|undefined),
 *       selected: boolean,
 *       title: string,
 *       value: string,
 *   }>
 * }
 */
export let SelectListType;

/**
 * Sets up given "select" element using the list and adds callback.
 * Creates option groups if needed.
 * @param {!Element} select Select object to be updated.
 * @param {!SelectListType} list List of the options to be added. Elements with
 *     optionGroupName are considered option group.
 * @param {?function(string)} callback Callback which should be called, or null
 *     if the event listener shouldn't be added.
 *
 * Note: do not forget to update getSelectedTitle() below if this is
 * updated!
 */
export function setupSelect(select, list, callback) {
  select.innerHTML = window.trustedTypes.emptyHTML;
  let optgroup = select;
  for (let i = 0; i < list.length; ++i) {
    const item = list[i];
    if (item.optionGroupName) {
      optgroup = document.createElement('optgroup');
      optgroup.label = item.optionGroupName;
      select.appendChild(optgroup);
    } else {
      const option =
          new Option(item.title, item.value, item.selected, item.selected);
      optgroup.appendChild(option);
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
 * @param {!SelectListType} list The same as in setupSelect() above.
 * @return {string}
 */
export function getSelectedTitle(list) {
  let firstTitle = '';
  for (let i = 0; i < list.length; ++i) {
    const item = list[i];
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
 * @param {!SelectListType} list The same as in setupSelect() above.
 * @return {?string}
 */
export function getSelectedValue(list) {
  for (let i = 0; i < list.length; ++i) {
    const item = list[i];
    if (item.optionGroupName) {
      continue;
    }
    if (item.selected) {
      return item.value;
    }
  }
  return null;
}
