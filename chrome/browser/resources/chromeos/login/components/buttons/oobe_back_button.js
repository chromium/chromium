// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* #js_imports_placeholder */

/**
 * @polymer
 */
/* #export */ class OobeBackButton extends OobeBaseButton {
  static get is() {
    return 'oobe-back-button';
  }

  /* #html_template_placeholder */

  static get properties() {
    return {
      /* The ID of the localized string to be used as button text.
       */
      textKey: {
        type: String,
        value: 'back',
      },
    };
  }
}

customElements.define(OobeBackButton.is, OobeBackButton);
