// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {InfobarInternalsAppElement} from './app.js';

export function getHtml(this: InfobarInternalsAppElement) {
  return html`<!--_html_template_start_-->
<cr-toolbar page-name="InfoBar Internals" role="banner"
    .showSearch="${false}">
</cr-toolbar>

<main id="container">
  <div class="page-header">
    <h2 class="page-title">Infobar Trigger Page</h2>
    <p class="page-description">
      Trigger infobars on demand to manually test how they look and behave.
      Pick the infobars you want from the list, then use the buttons on each
      card to show them.
    </p>
  </div>

  <section class="selector-section" aria-labelledby="selectorHeading">
    <h3 id="selectorHeading" class="section-heading">Infobars</h3>
    <div class="selector-row">
      <cr-button id="dropdownButton" class="action-button" aria-haspopup="menu"
          @click="${this.onDropdownClick}">
        ${this.selectedTypes.length === 0 ? html`
          <cr-icon slot="prefix-icon" icon="cr:add"></cr-icon>
        ` : ''}
        ${this.getDropdownLabel()}
        <cr-icon slot="suffix-icon" icon="cr:arrow-drop-down"></cr-icon>
      </cr-button>
      ${this.selectedTypes.length > 0 ? html`
        <cr-button id="clearButton" @click="${this.onClearSelectionClick}">
          Clear selection
        </cr-button>
      ` : ''}
    </div>
  </section>

  <cr-action-menu id="menu" aria-roledescription="Infobar list">
    <div class="menu-search">
      <cr-search-field label="Search infobars"
          clear-label="Clear search"
          @search-changed="${this.onSearchChanged}">
      </cr-search-field>
      <cr-button class="select-all-button"
          @click="${this.onSelectAllClick}">
        ${this.areAllFilteredSelected() ? 'Deselect all' : 'Select all'}
      </cr-button>
    </div>
    <div class="menu-items">
      ${this.getFilteredInfobars().length === 0 ? html`
        <div class="menu-empty">
          No infobars found matching "${this.searchQuery}"
        </div>
      ` : ''}
      ${this.getFilteredInfobars().map(infobar => html`
        <cr-checkbox class="dropdown-item"
            data-type="${infobar.type}"
            ?checked="${this.isSelected(infobar.type)}"
            @change="${this.onSelectionChange}">
          ${infobar.name}
        </cr-checkbox>
      `)}
    </div>
  </cr-action-menu>

  <section class="results-section" aria-labelledby="resultsHeading">
    <h3 id="resultsHeading" class="section-heading">
      Selected infobars (${this.selectedTypes.length})
    </h3>
    ${this.getSelectedInfobars().length === 0 ? html`
      <div class="empty-state">
        <cr-icon class="empty-state-icon" icon="cr:info"></cr-icon>
        <p class="empty-state-title">No infobars selected</p>
        <p class="empty-state-subtitle">
          Select one or more infobars from the list above to trigger and test
          them.
        </p>
      </div>
    ` : html`
      <div class="card-list" role="list">
        ${this.getSelectedInfobars().map(infobar => html`
          <div class="infobar-card" role="listitem">
            <div class="card-content">
              <h4 class="infobar-name">${infobar.name}</h4>
              <div class="infobar-description">${infobar.description}</div>
            </div>
            <div class="actions">
              <cr-button
                class="action-button"
                data-type="${infobar.type}"
                data-name="${infobar.name}"
                @click="${this.onTriggerClick}">
                Trigger
              </cr-button>
            </div>
          </div>
        `)}
      </div>
    `}
  </section>
</main>

<cr-toast id="toast" duration="3000">
  <span>${this.toastMessage}</span>
</cr-toast>
<!--_html_template_end_-->`;
}
