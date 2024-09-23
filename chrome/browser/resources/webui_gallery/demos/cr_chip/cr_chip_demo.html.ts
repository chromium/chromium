// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {CrChipDemoElement} from './cr_chip_demo.js';

export function getHtml(this: CrChipDemoElement) {
  return html`
<h1>cr-chip</h1>
<div class="demos">
  <cr-chip>
    <cr-icon icon="cr:print"></cr-icon>
    Action
  </cr-chip>

  <cr-chip chip-role="link">
    <cr-icon icon="cr:print"></cr-icon>
    Action Link
  </cr-chip>

  <cr-chip>
    <cr-icon icon="cr:add"></cr-icon>
    Filter
  </cr-chip>

  <cr-chip selected>
    <cr-icon icon="cr:check"></cr-icon>
    Selected filter
  </cr-chip>

  <cr-chip disabled>
    <cr-icon icon="cr:clear"></cr-icon>
    Disabled filter
  </cr-chip>
</div>`;
}
