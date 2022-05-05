// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* #js_imports_placeholder */

/**
 * @polymer
 */
class HdIronIcon extends Polymer.Element {
  static get is() {
    return 'hd-iron-icon';
  }

  /* #html_template_placeholder */

  static get properties() {
    return {
      icon1x: String,
      icon2x: String,
      src1x: String,
      src2x: String,
    };
  }
}

customElements.define(HdIronIcon.is, HdIronIcon);
