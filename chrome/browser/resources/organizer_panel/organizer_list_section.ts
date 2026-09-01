// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './organizer_list_section_item.js';

import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './organizer_list_section.css.js';
import {getHtml} from './organizer_list_section.html.js';
import type {OrganizerListSectionClient, OrganizerListSectionDelegate} from './organizer_list_section_delegate.js';
import type {OrganizerListSectionItem} from './organizer_list_section_item.js';

export interface OrganizerListSectionElement {
  $: {
    header: HTMLElement,
    items: HTMLElement,
  };
}

export class OrganizerListSectionElement extends CrLitElement implements
    OrganizerListSectionClient {
  static get is() {
    return 'organizer-list-section';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      delegate: {type: Object},
      items: {type: Array},
    };
  }

  accessor delegate: OrganizerListSectionDelegate|null = null;
  accessor items: OrganizerListSectionItem[] = [];

  // The panel WebUI will remain loaded but invisible when the panel is closed.
  // While invisible, the WebUI will not receive update events from the browser,
  // so all sections need to refetch their data upon visibility change.
  private onVisibilityChange_: () => void = () => {
    if (document.visibilityState === 'visible') {
      this.updateItems_();
    }
  };

  override connectedCallback() {
    super.connectedCallback();
    document.addEventListener('visibilitychange', this.onVisibilityChange_);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    document.removeEventListener('visibilitychange', this.onVisibilityChange_);
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('delegate')) {
      this.delegate?.init(this);
      this.updateItems_();
    }
  }

  onItemsChanged(items: OrganizerListSectionItem[]) {
    this.items = items;
  }

  private async updateItems_() {
    if (!this.delegate) {
      this.items = [];
      return;
    }
    this.items = await this.delegate.getItems();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'organizer-list-section': OrganizerListSectionElement;
  }
}

customElements.define(
    OrganizerListSectionElement.is, OrganizerListSectionElement);
