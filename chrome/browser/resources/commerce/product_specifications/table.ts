// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';
import './product_selector.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {UrlListEntry} from './product_selector.js';
import {getTemplate} from './table.html.js';

/** Describes a row in a ProductSpecs table. */
export interface TableRow {
  title: string;
  values: string[];
}
/** Describes a column in a ProductSpecs table. */
export interface TableColumn {
  selectedItem: UrlListEntry;
}

/** Element for rendering a ProductSpecs table. */
export class TableElement extends PolymerElement {
  static get is() {
    return 'product-specifications-table';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      columns: Array,
      rows: Array,
    };
  }

  columns: TableColumn[];
  rows: TableRow[];
}

declare global {
  interface HTMLElementTagNameMap {
    'product-specifications-table': TableElement;
  }
}

customElements.define(TableElement.is, TableElement);
