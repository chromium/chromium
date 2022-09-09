// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {findAncestor} from 'chrome://resources/js/util.m.js';

/**
 * @param {Node} el A node to search for ancestors with |className|.
 * @param {string} className A class to search for.
 * @return {Element} A node with class of |className| or null if none is found.
 */
export function findAncestorByClass(el, className) {
  return /** @type {Element} */ (findAncestor(el, function(el) {
    return el.classList && el.classList.contains(className);
  }));
}
