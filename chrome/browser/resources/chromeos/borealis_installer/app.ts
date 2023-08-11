// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';

export class BorealisInstallerAppElement extends PolymerElement {
  static get is() {
    return 'borealis-installer-app';
  }

  static get template() {
    return getTemplate();
  }

  onInstallButtonClick() {}
}


declare global {
  interface HTMLElementTagNameMap {
    'borealis-installer-app': BorealisInstallerAppElement;
  }
}

customElements.define(
    BorealisInstallerAppElement.is, BorealisInstallerAppElement);
