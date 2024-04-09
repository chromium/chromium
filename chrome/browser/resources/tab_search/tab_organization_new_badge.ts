// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/mwb_shared_style.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './tab_organization_new_badge.html.js';

// New badge divider for the tab organization UI.
export class TabOrganizationNewBadgeElement extends PolymerElement {
  static get is() {
    return 'tab-organization-new-badge';
  }

  static get template() {
    return getTemplate();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'tab-organization-new-badge': TabOrganizationNewBadgeElement;
  }
}

customElements.define(
    TabOrganizationNewBadgeElement.is, TabOrganizationNewBadgeElement);
