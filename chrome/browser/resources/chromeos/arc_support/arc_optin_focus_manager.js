// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Manages focus for ARC OptIn page.
 * @constructor
 * @extends {cr.ui.FocusManager}
 */
function ArcOptInFocusManager() {
  cr.ui.FocusManager.call(this);
}

ArcOptInFocusManager.prototype = {
  __proto__: cr.ui.FocusManager.prototype,

  /** @override */
  getFocusParent() {
    const overlay = $('overlay-container');
    if (overlay.hidden) {
      return document.body;
    } else {
      return $('overlay-page');
    }
  },
};
