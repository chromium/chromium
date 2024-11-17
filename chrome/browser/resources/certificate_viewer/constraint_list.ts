// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './constraint_list.css.js';
import {getHtml} from './constraint_list.html.js';


export class ConstraintListElement extends CrLitElement {
  static get is() {
    return 'constraint-list';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      constraints: {type: Array},
    };
  }

  constraints: string[] = [];
}

declare global {
  interface HTMLElementTagNameMap {
    'constraint-list': ConstraintListElement;
  }
}

customElements.define(ConstraintListElement.is, ConstraintListElement);
