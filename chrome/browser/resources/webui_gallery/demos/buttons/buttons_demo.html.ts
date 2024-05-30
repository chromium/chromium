// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ButtonsDemoElement} from './buttons_demo.js';

export function getHtml(this: ButtonsDemoElement) {
  return html`
<h1>cr-button</h1>
<div class="demos">
  <cr-button>Outline button</cr-button>
  <cr-button class="action-button">Primary button</cr-button>
  <cr-button class="tonal-button">Tonal button</cr-button>
  <cr-button disabled>Disabled outline button</cr-button>
  <cr-button disabled class="action-button">Disabled primary button</cr-button>
  <cr-button disabled class="tonal-button">Disabled tonal button</cr-button>
  <cr-button disabled class="floating-button">
    Disabled floating button
  </cr-button>
  <div class="flex">
    <cr-button class="cancel-button">Cancel</cr-button>
    <cr-button class="action-button">Confirm</cr-button>
  </div>
  <div class="row">
    <cr-button>
      Outline button with icon
      <cr-icon icon="cr:open-in-new" slot="suffix-icon"></cr-icon>
    </cr-button>
    <cr-button>
      <cr-icon icon="cr:add" slot="prefix-icon"></cr-icon>
      Outline button with icon
    </cr-button>
  </div>
  <div class="row">
    <cr-button class="action-button">
      <cr-icon icon="cr:add" slot="prefix-icon"></cr-icon>
      Primary button with icon
    </cr-button>
    <cr-button class="tonal-button">
      <cr-icon icon="cr:add" slot="prefix-icon"></cr-icon>
      Tonal button with icon
    </cr-button>
  </div>
  <div class="row">
    <cr-button class="floating-button">
      <cr-icon icon="cr:add" slot="prefix-icon"></cr-icon>
      Floating button with icon
    </cr-button>
  </div>
</div>

<h1>cr-icon-button</h1>
<div class="demos">
  <div class="row">
    <cr-icon-button iron-icon="cr:delete"></cr-icon-button>
    <cr-icon-button iron-icon="cr:delete" disabled></cr-icon-button>
  </div>
</div>

<h1>cr-expand-button</h1>
<div class="demos">
  <cr-expand-button ?expanded="${this.expanded_}"
      @expanded-changed="${this.onExpandedChanged_}"
      expand-title="Expand" collapse-title="Collapse">
    <div ?hidden="${this.expanded_}">Expand row</div>
    <div ?hidden="${!this.expanded_}">Collapse row</div>
  </cr-expand-button>
  <cr-collapse ?opened="${this.expanded_}">Some content goes here.</cr-collapse>

  <cr-expand-button expand-icon="cr:arrow-drop-down"
      collapse-icon="cr:arrow-drop-up">
    With custom icons
  </cr-expand-button>
  <cr-expand-button no-hover>
    With no hover effect on entire row
  </cr-expand-button>
</div>`;
}
