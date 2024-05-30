// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {CardDemoElement} from './card_demo.js';

export function getHtml(this: CardDemoElement) {
  return html`
<div class="cr-centered-card-container">
  <h2>Cards and rows with content</h2>
  <div class="card">
    <div class="cr-row first">
      <div class="cr-padded-text">This is an example of a row.</div>
    </div>

    <div class="cr-row">
      <div class="cr-padded-text">
        <div>Some title text</div>
        <div class="cr-secondary-text">Some secondary text</div>
      </div>
    </div>

    <div class="cr-row">
      <div class="cr-padded-text flex">
        Some text that takes up most of the row's space
      </div>
      <cr-button class="action-button">Button 1</cr-button>
      <cr-button class="cr-button-gap">Button 2</cr-button>
    </div>

    <div class="cr-row">
      A row...
    </div>
    <div id="continuationRow" class="cr-row continuation">
      ...that continues into another row.
    </div>
  </div>

  <h2>Cards with &lt;cr-link-row&gt; instances</h2>
  <div class="card">
    <cr-link-row label="A full length link row"></cr-link-row>
    <cr-link-row class="hr" label="A full length link row"
        sub-label="With some secondary text">
    </cr-link-row>
    <cr-link-row class="hr" external
        label="A row that links to an external website"
        @click="${this.onExternalLinkClick_}"></cr-link-row>
    <cr-link-row no-hover class="hr" label="Link row with no hover effect">
    </cr-link-row>
    <cr-link-row start-icon="cr:check" label="Row with an icon"></cr-link-row>
    <cr-link-row class="hr" using-slotted-label>
      <div slot="label">A link row that uses slotted content</div>
      <div slot="sub-label">And slotted sublabel</div>
      <cr-icon icon="cr:check"></cr-icon>
    </cr-link-row>
  </div>

  <h2>Other examples</h2>
  <div class="card">
    <cr-expand-button class="cr-row first" ?expanded="${this.expanded_}"
        @expanded-changed="${this.onExpandedChanged_}">
      A row that expands...
    </cr-expand-button>
    <cr-collapse id="expandedContent" ?opened="${this.expanded_}">
      <div class="cr-padded-text">...into more rows!</div>
      <div class="cr-padded-text hr">...into more rows!</div>
      <div class="cr-padded-text hr">...into more rows!</div>
    </cr-collapse>
  </div>
</div>`;
}
