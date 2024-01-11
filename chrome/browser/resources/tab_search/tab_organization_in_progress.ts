// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';
import './tab_organization_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_loading_gradient/cr_loading_gradient.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './tab_organization_in_progress.html.js';

export interface TabOrganizationInProgressElement {
  $: {
    header: HTMLElement,
  };
}

// Loading state for the tab organization UI.
export class TabOrganizationInProgressElement extends PolymerElement {
  static get is() {
    return 'tab-organization-in-progress';
  }

  static get template() {
    return getTemplate();
  }

  announceHeader() {
    this.$.header.textContent = '';
    this.$.header.textContent = this.getTitle_();
  }

  private getTitle_(): string {
    return loadTimeData.getString('inProgressTitle');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'tab-organization-in-progress': TabOrganizationInProgressElement;
  }
}

customElements.define(
    TabOrganizationInProgressElement.is, TabOrganizationInProgressElement);
