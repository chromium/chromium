// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_loading_gradient/cr_loading_gradient.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './tab_organization_in_progress.css.js';
import {getHtml} from './tab_organization_in_progress.html.js';

export interface TabOrganizationInProgressElement {
  $: {
    header: HTMLElement,
  };
}

// Loading state for the tab organization UI.
export class TabOrganizationInProgressElement extends CrLitElement {
  static get is() {
    return 'tab-organization-in-progress';
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
    'tab-organization-in-progress': TabOrganizationInProgressElement;
  }
}

customElements.define(
    TabOrganizationInProgressElement.is, TabOrganizationInProgressElement);
