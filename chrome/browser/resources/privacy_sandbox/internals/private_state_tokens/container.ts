// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import './list_item.js';

import type {CrButtonElement} from '//resources/cr_elements/cr_button/cr_button.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './container.css.js';
import {getHtml} from './container.html.js';
import {dummyListItemData} from './test_data.js';
import type {ListItem} from './test_data.js';

export interface PrivateStateTokensContainerElement {
  $: {
    expandCollapseButton: CrButtonElement,
  };
}

export class PrivateStateTokensContainerElement extends CrLitElement {
  static get is() {
    return 'private-state-tokens-container';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      data: {type: Array},
      isAnyRowCollapsed: {type: Boolean},
    };
  }

  data: ListItem[] = dummyListItemData;  // TODO: use real data and type
  protected isAnyRowCollapsed: boolean = true;

  private rowExpandedStates_: Map<string, boolean> = new Map();

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);
    this.isAnyRowCollapsed = Array.from(this.rowExpandedStates_.values())
                                 .some(expanded => !expanded);
  }

  protected onClick_() {
    const rows =
        this.shadowRoot!.querySelectorAll('private-state-tokens-list-item');
    for (const row of rows) {
      (row).expanded =
          this.isAnyRowCollapsed;
    }
  }

  protected onExpandedToggled_(e: CustomEvent<{
    id: string,
    expanded: boolean,
  }>) {
    this.rowExpandedStates_.set(e.detail.id, e.detail.expanded);
    this.requestUpdate();
  }

  protected expandCollapseButtonText_(): string {
    return this.isAnyRowCollapsed ? 'Expand All' : 'Collapse All';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'private-state-tokens-container': PrivateStateTokensContainerElement;
  }
}

customElements.define(
    PrivateStateTokensContainerElement.is, PrivateStateTokensContainerElement);
