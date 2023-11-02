// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* #js_imports_placeholder */

/**
 * @polymer
 */
/* #export */ class OobeTextButton extends OobeBaseButton {

  static get is() {
    return 'oobe-text-button';
  }

  /* #html_template_placeholder */

  static get properties() {
    return {
      inverse: {
        type: Boolean,
        observer: 'onInverseChanged_',
      },

      border: Boolean,
    };
  }

  onInverseChanged_() {
    this.$.button.classList.toggle('action-button', this.inverse);
  }
}

customElements.define(OobeTextButton.is, OobeTextButton);
