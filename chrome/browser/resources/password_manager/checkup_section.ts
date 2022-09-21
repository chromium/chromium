// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/icons.html.js';
import './shared_style.css.js';

import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './checkup_section.html.js';

export class CheckupSectionElement extends PolymerElement {
  static get is() {
    return 'checkup-section';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The number of checked passwords as a formatted string.
       */
      checkedPasswordsText_: String,

      /**
       * The number of compromised passwords as a formatted string.
       */
      compromisedPasswordsText_: String,

      /**
       * The number of weak passwords as a formatted string.
       */
      reusedPasswordsText_: String,

      /**
       * The number of weak passwords as a formatted string.
       */
      weakPasswordsText_: String,
    };
  }

  private checkedPasswordsText_: string;
  private compromisedPasswordsText_: string;
  private reusedPasswordsText_: string;
  private weakPasswordsText_: string;

  override ready() {
    super.ready();
    this.fetchPluralizedStrings_();
  }

  private fetchPluralizedStrings_() {
    const proxy = PluralStringProxyImpl.getInstance();

    proxy.getPluralString('checkedPasswords', 6)
        .then(result => this.checkedPasswordsText_ = result);

    proxy.getPluralString('compromisedPasswords', 2)
        .then(result => this.compromisedPasswordsText_ = result);

    proxy.getPluralString('reusedPasswords', 0)
        .then(result => this.reusedPasswordsText_ = result);

    proxy.getPluralString('weakPasswords', 4)
        .then(result => this.weakPasswordsText_ = result);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'checkup-section': CheckupSectionElement;
  }
}

customElements.define(CheckupSectionElement.is, CheckupSectionElement);
