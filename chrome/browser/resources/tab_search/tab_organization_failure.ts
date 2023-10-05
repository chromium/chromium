// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './tab_organization_shared_style.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './tab_organization_failure.html.js';

// Failure state for the tab organization UI.
export class TabOrganizationFailureElement extends PolymerElement {
  static get is() {
    return 'tab-organization-failure';
  }

  static get template() {
    return getTemplate();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'tab-organization-failure': TabOrganizationFailureElement;
  }
}

customElements.define(
    TabOrganizationFailureElement.is, TabOrganizationFailureElement);
