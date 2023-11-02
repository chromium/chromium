// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * TODO(crbug.com/1229130) - Find a better way to do this.
 *
 * Code which is embedded inside of the enterprise enrollment webview.
 * See /screens/oobe/enterprise_enrollment.js for details.
 */
const KEYBOARD_UTILS_FOR_INJECTION = String.raw`
                    (function() {
                       // <include src="../../keyboard/keyboard_utils.js">
                       keyboard.initializeKeyboardFlow(true);
                     })();`;

window.KEYBOARD_UTILS_FOR_INJECTION = KEYBOARD_UTILS_FOR_INJECTION;