// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_lazy_list/cr_lazy_list.js';

import type {Uuid} from '//resources/mojo/mojo/public/mojom/base/uuid.mojom-webui.js';
import type {Url} from '//resources/mojo/url/mojom/url.mojom-webui.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './comparison_table_list.css.js';
import {getHtml} from './comparison_table_list.html.js';

export interface ComparisonTableDetails {
  name: string;
  uuid: Uuid;
  numUrls: number;
  imageUrl: Url|null;
}

export class ComparisonTableListElement extends CrLitElement {
  static get is() {
    return 'comparison-table-list';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      tables: {type: Array},
    };
  }

  tables: ComparisonTableDetails[] = [];
}

declare global {
  interface HTMLElementTagNameMap {
    'comparison-table-list': ComparisonTableListElement;
  }
}

customElements.define(
    ComparisonTableListElement.is, ComparisonTableListElement);
