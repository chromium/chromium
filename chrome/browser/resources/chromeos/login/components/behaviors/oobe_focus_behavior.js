// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {afterNextRender} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @fileoverview
 * 'OobeFocusBehavior' is a special behavior which supports focus transferring
 * when a new screen is shown.
 */

/** @polymerBehavior */
export const OobeFocusBehavior = {
  /**
   * @private
   * Focuses the element. As cr-input uses focusInput() instead of focus() due
   * to bug, we have to handle this separately.
   * TODO(crbug.com/882612): Replace this with focus() in focusMarkedElement().
   */
  focusOnElement_(element) {
    if (element.focusInput) {
      element.focusInput();
      return;
    }
    element.focus();
  },

  /**
   * Called when the screen is shown to handle initial focus.
   */
  focusMarkedElement(root) {
    if (!root) {
      return;
    }
    var focusedElements = root.getElementsByClassName('focus-on-show');
    var focused = false;
    for (var i = 0; i < focusedElements.length; ++i) {
      if (focusedElements[i].hidden) {
        continue;
      }

      focused = true;
      afterNextRender(this, () => this.focusOnElement_(focusedElements[i]));
      break;
    }
    if (!focused && focusedElements.length > 0) {
      afterNextRender(this, () => this.focusOnElement_(focusedElements[0]));
    }

    this.fire('show-dialog');
  },
};

/**
 * TODO: Replace with an interface. b/24294625
 * @typedef {{
 *   focusMarkedElement: function()
 * }}
 */
OobeFocusBehavior.Proto;

/** @interface */
export class OobeFocusBehaviorInterface {
  focusMarkedElement(root) {}
}
