// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './tab_organization_not_started_image.html.js';

// Themed image for the tab organization not started state.
export class TabOrganizationNotStartedImageElement extends PolymerElement {
  static get is() {
    return 'tab-organization-not-started-image';
  }

  static get template() {
    return getTemplate();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'tab-organization-not-started-image': TabOrganizationNotStartedImageElement;
  }
}

customElements.define(
    TabOrganizationNotStartedImageElement.is,
    TabOrganizationNotStartedImageElement);
