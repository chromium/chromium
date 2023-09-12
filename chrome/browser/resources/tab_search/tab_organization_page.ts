// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/mwb_shared_style.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './tab_organization_page.html.js';

export class TabOrganizationPageElement extends PolymerElement {
  static get is() {
    return 'tab-organization-page';
  }

  static get properties() {
    return {
      title_: String,
    };
  }

  private title_: string = 'Placeholder';

  static get template() {
    return getTemplate();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'tab-organization-page': TabOrganizationPageElement;
  }
}

customElements.define(
    TabOrganizationPageElement.is, TabOrganizationPageElement);
