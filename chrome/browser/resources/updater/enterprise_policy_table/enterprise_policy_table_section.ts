// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_collapse/cr_collapse.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/icons.html.js';
import './enterprise_policy_value.js';
import '../icons.html.js';

import {assert} from '//resources/js/assert.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import type {PolicyData} from '../event_history.js';

import {getCss} from './enterprise_policy_table_section.css.js';
import {getHtml} from './enterprise_policy_table_section.html.js';

export interface RowData {
  name: string;
  policy: PolicyData;
  isExpanded: boolean;
  hasConflict: boolean;
}

export class EnterprisePolicyTableSectionElement extends CrLitElement {
  static get is() {
    return 'enterprise-policy-table-section';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      rowData: {type: Object},
    };
  }

  accessor rowData: RowData[] = [];

  protected canExpand(item: RowData): boolean {
    return Object.keys(item.policy.valuesBySource).length > 1;
  }

  protected onExpandButtonClick(e: Event) {
    const currentTarget = e.currentTarget as HTMLElement;
    const index = Number(currentTarget.dataset['index']);
    assert(!Number.isNaN(index));

    const data = this.rowData[index];
    assert(data !== undefined);
    data.isExpanded = !data.isExpanded;
    this.requestUpdate();
  }

  protected prevailingValue(item: RowData): unknown {
    return item.policy.valuesBySource[item.policy.prevailingSource];
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'enterprise-policy-table-section': EnterprisePolicyTableSectionElement;
  }
}

customElements.define(
    EnterprisePolicyTableSectionElement.is,
    EnterprisePolicyTableSectionElement);
