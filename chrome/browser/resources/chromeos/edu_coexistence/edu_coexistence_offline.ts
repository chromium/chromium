// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './edu_coexistence_template.js';
import './edu_coexistence_button.js';
import './common.css.js';
import '../supervision/supervised_user_offline.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {EduCoexistenceBrowserProxyImpl} from './edu_coexistence_browser_proxy.js';
import {getTemplate} from './edu_coexistence_offline.html.js';

class EduCoexistenceOffline extends PolymerElement {
  static get is() {
    return 'edu-coexistence-offline';
  }

  static get template() {
    return getTemplate();
  }

  override ready() {
    super.ready();
    const template =
        this.shadowRoot!.querySelector('edu-coexistence-template')!;
    template.updateButtonFooterVisibility(true);

    this.addEventListener('go-action', () => {
      this.closeDialog();
    });
  }

  /**
   * Attempts to close the dialog. In OOBE, this will move on
   * to the next screen of OOBE (not the next screen of this flow).
   */
  private closeDialog() {
    EduCoexistenceBrowserProxyImpl.getInstance().dialogClose();
  }
}

customElements.define(EduCoexistenceOffline.is, EduCoexistenceOffline);
