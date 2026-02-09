// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './filter_dialog.js';

import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {loadTimeData} from '../../i18n_setup.js';
import {FilterCategory, getFilterCategoryForTarget} from '../filter_bar.js';

import {getCss} from './type_dialog.css.js';
import {getHtml} from './type_dialog.html.js';

export class TypeDialogElement extends CrLitElement {
  static get is() {
    return 'type-dialog';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      anchorElement: {type: Object},
    };
  }

  accessor anchorElement: HTMLElement|null = null;

  protected readonly filterMenuItems:
      Array<{filterCategory: FilterCategory, label: string}> = [
        {
          filterCategory: FilterCategory.APP,
          label: loadTimeData.getString('app'),
        },
        {
          filterCategory: FilterCategory.EVENT,
          label: loadTimeData.getString('eventType'),
        },
        {
          filterCategory: FilterCategory.OUTCOME,
          label: loadTimeData.getString('updateOutcome'),
        },
        {
          filterCategory: FilterCategory.SCOPE,
          label: loadTimeData.getString('scope'),
        },
        {
          filterCategory: FilterCategory.DATE,
          label: loadTimeData.getString('date'),
        },
      ];

  override firstUpdated(changedProperties: PropertyValues<this>) {
    super.firstUpdated(changedProperties);
    this.shadowRoot.querySelector<HTMLElement>('.filter-menu-item')?.focus();
  }

  protected onClick(e: MouseEvent) {
    this.fire(
        'type-selection-changed',
        getFilterCategoryForTarget(e.target as HTMLElement));
  }

  protected onClose() {
    this.fire('close');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'type-dialog': TypeDialogElement;
  }
}

customElements.define(TypeDialogElement.is, TypeDialogElement);
