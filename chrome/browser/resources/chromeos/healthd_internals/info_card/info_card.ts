// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/ash/common/cr_elements/cr_button/cr_button.js';
import '//resources/ash/common/cr_elements/cr_expand_button/cr_expand_button.js';
import '//resources/ash/common/cr_elements/cr_page_host_style.css.js';
import '//resources/ash/common/cr_elements/cr_shared_vars.css.js';
import '//resources/polymer/v3_0/iron-collapse/iron-collapse.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './info_card.html.js';

interface DisplayedCardInfo {
  infoHeader: string;
  detailedInfo: string;
  isExpanded: boolean;
}

export class HealthdInternalsInfoCardElement extends PolymerElement {
  static get is() {
    return 'healthd-internals-info-card';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      displayedInfoList: {type: Array},
    };
  }

  private displayedInfoList: DisplayedCardInfo[] = [];

  // Append a new row in the displayed card.
  appendCardRow(header: string) {
    this.displayedInfoList.push({
      infoHeader: header,
      detailedInfo: '',
      isExpanded: false,
    });
  }

  // Refresh card components for new rows added after the UI is rendered.
  refreshComponents() {
    // Create a copy to trigger a change.
    this.set('displayedInfoList', this.displayedInfoList.slice());
  }

  // Update the data at the assigned row.
  updateDisplayedInfo(rowIndex: number, data: any) {
    if (rowIndex > this.displayedInfoList.length) {
      console.warn(
          `Invalid row index: ${rowIndex}. Call appendCardRow() first.`);
      return;
    }
    // Update the property by the `set` function to trigger a change.
    this.set(
        `displayedInfoList.${rowIndex}.detailedInfo`,
        this.encodeJsonString(data));
  }

  private encodeJsonString(data: any) {
    return JSON.stringify(data, null, 2);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'healthd-internals-info-card': HealthdInternalsInfoCardElement;
  }
}

customElements.define(
    HealthdInternalsInfoCardElement.is, HealthdInternalsInfoCardElement);
