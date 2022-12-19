// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Blue header for New Gaia UI, contains blue avatar logo and user
 * email.
 *
 * Example:
 *   <gaia-header email="user@example.com">
 *   </gaia-header>
 *
 * Attributes:
 *  'email' - displayed email.
 */

import './common_styles/oobe_common_styles.css.js';

import {html, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/** @polymer */
class GaiaHeader extends PolymerElement {
  static get is() {
    return 'gaia-header';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {email: String};
  }
}

customElements.define(GaiaHeader.is, GaiaHeader);
