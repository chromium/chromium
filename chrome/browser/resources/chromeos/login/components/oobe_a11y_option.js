// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* #js_imports_placeholder */

/* #export */ class OobeA11yOption extends Polymer.Element {
  static get is() {
    return 'oobe-a11y-option';
  }

  /* #html_template_placeholder */

  static get properties() {
    return {
      /**
       * If cr-toggle is checked.
       */
      checked: {
        type: Boolean,
      },

      /**
       * Chrome message handling this option.
       */
      chromeMessage: {
        type: String,
      },

      /**
       * ARIA-label for the button.
       *
       * Note that we are not using "aria-label" property here, because
       * we want to pass the label value but not actually declare it as an
       * ARIA property anywhere but the actual target element.
       */
      labelForAria: {
        type: String,
      },
    };
  }

  focus() {
    this.$.button.focus();
  }
}

customElements.define(OobeA11yOption.is, OobeA11yOption);