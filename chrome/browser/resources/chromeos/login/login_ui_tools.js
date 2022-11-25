// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview JS helpers used on login.
 */

  /**
   * Listens to key events on input element.
   * @param {Element} element DOM element
   * @param {Object} callback
   */
  export function addSubmitListener(element, callback) {
    element.addEventListener('keydown', (function(callback, e) {
                                          if (e.keyCode != 13) {
                                            return;
                                          }
                                          callback();
                                        }).bind(undefined, callback));
  }
