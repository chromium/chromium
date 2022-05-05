// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* #js_imports_placeholder */

/**
 * @polymer
 */
class OobeSlide extends Polymer.Element {

  static get is() {
    return 'oobe-slide';
  }

  /* #html_template_placeholder */

  static get properties() {
    return {
      /* If true title is colored red, else blue.
       */
      isWarning: {
        type: Boolean,
        value: false,
      },
    };
  }
}

customElements.define(OobeSlide.is, OobeSlide);