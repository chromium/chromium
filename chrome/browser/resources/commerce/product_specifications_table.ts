// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import './strings.m.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './product_specifications_table.html.js';

/** Describes a row in a ProductSpecs table. */
export interface TableRow {
  title: string;
  values: string[];
}
/** Describes a column in a ProductSpecs table. */
export interface TableColumn {
  title: string;
}

/** Element for rendering a ProductSpecs table. */
export class ProductSpecificationsTable extends PolymerElement {
  static get is() {
    return 'product-specifications-table';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      columns: {type: Array},
      rows: {type: Array},
    };
  }

  columns: TableColumn[];
  rows: TableRow[];
}

declare global {
  interface HTMLElementTagNameMap {
    'product-specifications-table': ProductSpecificationsTable;
  }
}

customElements.define(
    ProductSpecificationsTable.is, ProductSpecificationsTable);
