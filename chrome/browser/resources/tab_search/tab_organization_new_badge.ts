// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './tab_organization_new_badge.css.js';
import {getHtml} from './tab_organization_new_badge.html.js';

// New badge divider for the tab organization UI.
export class TabOrganizationNewBadgeElement extends CrLitElement {
  static get is() {
    return 'tab-organization-new-badge';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'tab-organization-new-badge': TabOrganizationNewBadgeElement;
  }
}

customElements.define(
    TabOrganizationNewBadgeElement.is, TabOrganizationNewBadgeElement);
