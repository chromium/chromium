// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Defines a global object. The initialization of this
 *   object happens in init.js.
 *
 */

// Forward declare.
goog.addDependency('abstract_earcons.js', ['AbstractEarcons'], []);
goog.addDependency('braille_interface.js', ['BrailleInterface'], []);
goog.addDependency('tts_interface.js', ['TtsInterface'], []);

goog.provide('ChromeVox');

goog.require('constants');

/**
 * @constructor
 */
ChromeVox = function() {};

// Constants
/**
 * Constant for verbosity setting (ChromeVox.verbosity).
 * @enum {number}
 */
ChromeVox.VerbosityType = {
  VERBOSE: 0,
  BRIEF: 1
};

/**
 * @type {TtsInterface}
 */
ChromeVox.tts;
/**
 * @type {BrailleInterface}
 */
ChromeVox.braille;
/**
 * @type {?string}
 */
ChromeVox.version = null;
/**
 * @type {AbstractEarcons}
 */
ChromeVox.earcons = null;
/**
 * This indicates whether or not the sticky mode pref is toggled on.
 * Use ChromeVox.isStickyModeOn() to test if sticky mode is enabled
 * either through the pref or due to being temporarily toggled on.
 * @type {boolean}
 */
ChromeVox.isStickyPrefOn = false;
/**
 * If set to true or false, this value overrides ChromeVox.isStickyPrefOn
 * temporarily - in order to temporarily enable sticky mode while doing
 * 'read from here' or to temporarily disable it while using a widget.
 * @type {?boolean}
 */
ChromeVox.stickyOverride = null;
/**
 * @type {boolean}
 */
ChromeVox.keyPrefixOn = false;
/**
 * Verbosity setting.
 * See: VERBOSITY_VERBOSE and VERBOSITY_BRIEF
 * @type {number}
 */
ChromeVox.verbosity = ChromeVox.VerbosityType.VERBOSE;
/**
 * @type {number}
 */
ChromeVox.typingEcho = 0;
/**
 * Echoing on key press events.
 * @type {Object<boolean>}
 */
ChromeVox.keyEcho = {};
/**
 * @type {Object<string, constants.Point>}
 */
ChromeVox.position = {};
/**
 * @type {string}
 */
ChromeVox.modKeyStr = 'Shift+Search';
/**
 * If any of these keys is pressed with the modifier key, we go in sequence mode
 * where the subsequent independent key downs (while modifier keys are down)
 * are a part of the same shortcut. This array is populated in
 * ChromeVoxKbHandler.loadKeyToFunctionsTable().
 * @type {!Array<KeySequence>}
 */
ChromeVox.sequenceSwitchKeyCodes = [];

/**
 * Returns whether sticky mode is on, taking both the global sticky mode
 * pref and the temporary sticky mode override into account.
 *
 * @return {boolean} Whether sticky mode is on.
 */
ChromeVox.isStickyModeOn = function() {
  if (ChromeVox.stickyOverride !== null) {
    return ChromeVox.stickyOverride;
  } else {
    return ChromeVox.isStickyPrefOn;
  }
};

/**
 * Shortcut for document.getElementById.
 * @param {string} id of the element.
 * @return {Element} with the id.
 */
function $(id) {
  return document.getElementById(id);
}
