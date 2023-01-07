// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* #js_imports_placeholder */

/**
 * @polymer
 */
/* #export */ class OobeNextButton extends OobeBaseButton {
  static get is() {
    return 'oobe-next-button';
  }

  /* #html_template_placeholder */

  static get properties() {
    return {
      /* The ID of the localized string to be used as button text.
       */
      textKey: {
        type: String,
        value: 'next',
      },
    };
  }
}

customElements.define(OobeNextButton.is, OobeNextButton);
