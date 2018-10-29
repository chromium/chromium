// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'multidevice-radio-button',

  behaviors: [
    CrRadioButtonBehavior,
  ],

  /**
   * Prevents on-click handles on the control from being activated when the
   * indicator is clicked.
   * @param {!Event} e The click event.
   * @private
   */
  onIndicatorTap_: function(e) {
    e.preventDefault();
    e.stopPropagation();
  },
});
