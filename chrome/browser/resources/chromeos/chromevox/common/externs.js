// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @type {Object}
 */
chrome.brailleDisplayPrivate = {};

/**
 * @param {function(!{available: boolean, textRowCount: (number|undefined),
 *        textColumnCount: (number|undefined)})}
 *        callback
 */
chrome.brailleDisplayPrivate.getDisplayState = function(callback) {};

/**
 * @type {ChromeEvent}
 */
chrome.brailleDisplayPrivate.onDisplayStateChanged;

/**
 * @type {ChromeEvent}
 */
chrome.brailleDisplayPrivate.onKeyEvent;

/**
 * @param {ArrayBuffer} cells
 * @param {number} columns
 * @param {number} rows
 */
chrome.brailleDisplayPrivate.writeDots = function(cells, columns, rows) {};


/**
 * @param {string} address
 */
chrome.brailleDisplayPrivate.updateBluetoothBrailleDisplayAddress = function(
    address) {};

/**
 * @const
 */
chrome.virtualKeyboardPrivate = {};

/**
 * @typedef {{type: string, charValue: number, keyCode: number,
 *            keyName: string, modifiers: (number|undefined)}}
 */
chrome.virtualKeyboardPrivate.VirtualKeyboardEvent;

/**
 * @param {chrome.virtualKeyboardPrivate.VirtualKeyboardEvent} keyEvent
 * @param {Function=} opt_callback
 */
chrome.virtualKeyboardPrivate.sendKeyEvent = function(
    keyEvent, opt_callback) {};

/**
 * @param {function(!{a11ymode: boolean})} opt_callback
 */
chrome.virtualKeyboardPrivate.getKeyboardConfig = function(opt_callback) {};

/**
 * @type {Object}
 */
window.speechSynthesis;

/**
 * @type {Event}
 */
window.speechSynthesis.onvoiceschanged;
