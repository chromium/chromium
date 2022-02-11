// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/polymer/v3_0/paper-styles/color.js';
import './code_section.js';

import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

interface ExtensionsInstallWarningsDialogElement {
  $: {
    dialog: CrDialogElement,
  };
}

class ExtensionsInstallWarningsDialogElement extends PolymerElement {
  static get is() {
    return 'extensions-install-warnings-dialog';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      installWarnings: Array,
    };
  }

  installWarnings: Array<string>;

  connectedCallback() {
    super.connectedCallback();
    this.$.dialog.showModal();
  }

  private onOkTap_() {
    this.$.dialog.close();
  }
}

customElements.define(
    ExtensionsInstallWarningsDialogElement.is,
    ExtensionsInstallWarningsDialogElement);
