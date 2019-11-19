// Copyright 2018 The Chromium Authors. All rights reserved.
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
var SelectListType;

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
var setupSelect = function(select, list, callback) {
  select.innerHTML = '';
  var optgroup = select;
  for (var i = 0; i < list.length; ++i) {
    var item = list[i];
    if (item.optionGroupName) {
      optgroup = document.createElement('optgroup');
      optgroup.label = item.optionGroupName;
      select.appendChild(optgroup);
    } else {
      var option =
          new Option(item.title, item.value, item.selected, item.selected);
      optgroup.appendChild(option);
    }
  }
  if (callback) {
    var runCallback = function() {
      callback(select.options[select.selectedIndex].value);
    };
    select.addEventListener('blur', runCallback);
    select.addEventListener('change', runCallback);
    select.addEventListener('click', runCallback);
    select.addEventListener('input', runCallback);
    select.addEventListener('keyup', function(event) {
      var keycodeInterested = [
        9,   // Tab
        13,  // Enter
        27,  // Escape
        37,  // Left
        38,  // Up
        39,  // Right
        40,  // Down
      ];
      if (keycodeInterested.indexOf(event.keyCode) >= 0)
        runCallback();
    });
  }
};

/**
 * Returns title of the selected option (see setupSelect() above).
 * @param {!SelectListType} list The same as in setupSelect() above.
 * @return {string}
 */
var getSelectedTitle = function(list) {
  var firstTitle = '';
  for (var i = 0; i < list.length; ++i) {
    var item = list[i];
    if (item.optionGroupName)
      continue;

    if (!firstTitle)
      firstTitle = item.title;

    if (item.selected)
      return item.title;
  }
  return firstTitle;
};

/**
 * Returns value of the selected option (see setupSelect() above).
 * @param {!SelectListType} list The same as in setupSelect() above.
 * @return {?string}
 */
var getSelectedValue = function(list) {
  for (var i = 0; i < list.length; ++i) {
    var item = list[i];
    if (item.optionGroupName)
      continue;
    if (item.selected)
      return item.value;
  }
  return null;
};
