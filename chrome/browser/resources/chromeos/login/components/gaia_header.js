// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element wrapping gaia styled header for login/oobe.
 */

/* #js_imports_placeholder */

class GaiaHeader extends Polymer.Element {
  static get is() {
    return 'gaia-header';
  }

  /* #html_template_placeholder */

  static get properties() {
    return {email: String};
  }

  constructor() {
    super();
  }
}

customElements.define(GaiaHeader.is, GaiaHeader);
