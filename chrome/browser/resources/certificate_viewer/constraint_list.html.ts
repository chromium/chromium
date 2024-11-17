// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ConstraintListElement} from './constraint_list.js';

export function getHtml(this: ConstraintListElement) {
  return html`
  <div>
  ${
      this.constraints.map(
          constraint => html`<div class="constraint">${constraint}</div>`)}
  </div>`;
}
