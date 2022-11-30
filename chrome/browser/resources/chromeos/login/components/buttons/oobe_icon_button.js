// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/* #js_imports_placeholder */

/**
 * @polymer
 */
/* #export */ class OobeIconButton extends OobeBaseButton {

  static get is() {
    return 'oobe-icon-button';
  }

  /* #html_template_placeholder */

  static get properties() {
    return {
      icon1x: {type: String, observer: 'updateIconVisibility_'},
      icon2x: String,
    };
  }

  updateIconVisibility_() {
    this.$.icon.hidden = (this.icon1x === undefined || this.icon1x.length == 0);
  }

}

customElements.define(OobeIconButton.is, OobeIconButton);
