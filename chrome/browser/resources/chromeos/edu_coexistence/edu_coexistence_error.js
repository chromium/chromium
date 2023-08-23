// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './edu_coexistence_css.js';
import './edu_coexistence_template.js';
import './edu_coexistence_button.js';
import './supervision/supervised_user_error.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {EduCoexistenceBrowserProxyImpl} from './edu_coexistence_browser_proxy.js';

class EduCoexistenceError extends PolymerElement {
  static get is() {
    return 'edu-coexistence-error';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  /** @override */
  ready() {
    super.ready();
    this.shadowRoot.querySelector('edu-coexistence-template')
        .showButtonFooter(true);

    this.addEventListener('go-action', () => {
      this.closeDialog_();
    });
  }

  /**
   * Attempts to close the dialog. In OOBE, this will move on
   * to the next screen of OOBE (not the next screen of this flow).
   * @private
   */
  closeDialog_() {
    EduCoexistenceBrowserProxyImpl.getInstance().dialogClose();
  }
}

customElements.define(EduCoexistenceError.is, EduCoexistenceError);
