// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './tab_organization_not_started_image.css.js';
import {getHtml} from './tab_organization_not_started_image.html.js';

// Themed image for the tab organization not started state.
export class TabOrganizationNotStartedImageElement extends CrLitElement {
  static get is() {
    return 'tab-organization-not-started-image';
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
    'tab-organization-not-started-image': TabOrganizationNotStartedImageElement;
  }
}

customElements.define(
    TabOrganizationNotStartedImageElement.is,
    TabOrganizationNotStartedImageElement);
