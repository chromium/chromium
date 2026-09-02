// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {MemoryTabElement} from './memory_tab.js';

export function getHtml(this: MemoryTabElement) {
  return html`<!--_html_template_start_-->
<div class="table-container">
  <div class="toolbar">
    <cr-button @click="${this.onTogglePauseClick_}">
      ${this.isPaused_ ? 'Resume' : 'Pause'}
    </cr-button>
    <span class="status-text">
      ${this.isPaused_ ? 'Paused' : 'Refreshing every 2s'}
      ${this.lastUpdated_ ? html`• Last updated: ${this.lastUpdated_}` : ''}
    </span>
  </div>

  <table>
    <thead>
      <tr>
        <th class="pid-col" data-sort-key="pid" @click="${this.onSortClick}">
          PID
        </th>
        <th class="process-desc-col"
            data-sort-key="description"
            @click="${this.onSortClick}">
          Process Description
        </th>
        <th class="footprint-col"
            data-sort-key="privateFootprintKb"
            @click="${this.onSortClick}">
          Private Memory Footprint
        </th>
        <th class="magnitude-col">
          Magnitude
        </th>
        <th class="trend-col">
          Trend (20s)
        </th>
      </tr>
    </thead>
    <tbody>
      ${this.getSortedProcesses_().map(item => html`
        <tr class="process-row ${item.isDead ? 'dead' : ''}"
            data-pid="${item.pid}"
            @click="${this.onProcessRowClick_}">
          <td class="pid-col">${item.pid}</td>
          <td class="process-desc-col">
            <div class="process-desc-container">
              <span class="toggle-arrow ${
                  this.isExpanded_(item.pid) ? 'expanded' : ''}">▶</span>
              <span class="process-desc-text"
                    title="${item.description}">
                ${item.description}
              </span>
              ${item.isDead ?
                  html`<span class="dead-badge">Terminated</span>` : ''}
            </div>
          </td>
          <td class="footprint-col">${item.privateFootprintFormatted}</td>
          <td class="magnitude-col">
            <div class="magnitude-bar-track">
              <div class="magnitude-bar-fill"
                   style="width: ${item.magnitudePercent}%;"></div>
            </div>
          </td>
          <td class="trend-col">
            <div class="trend-sparkline">
              ${this.getSparklineBars(item.history, this.maxPrivateFootprintKb_).map(bar => html`
                <div class="sparkline-bar ${bar.isEmpty ? 'empty' : ''} ${
                    bar.isError ? 'error' : ''}"
                     style="height: ${bar.height}px;"
                     title="${bar.isEmpty ? '' :
                         (bar.isError ? 'Memory dump unavailable' :
                          (bar.value !== null ? bar.value.toLocaleString() : ''))}">
                </div>
              `)}
            </div>
          </td>
        </tr>
        ${this.isExpanded_(item.pid) ? html`
          <tr class="breakdown-wrapper-row">
            <td colspan="5" class="breakdown-wrapper-cell">
              <div class="breakdown-card">
                ${item.sections.map(section => html`
                  <div class="section-card">
                    <div class="section-header"
                         data-pid="${item.pid}"
                         data-section-id="${section.id}"
                         @click="${this.onSectionRowClick_}">
                      <div class="section-header-title">
                        <span class="toggle-arrow ${
                            this.isSectionExpanded_(item.pid, section.id) ?
                            'expanded' : ''}">▶</span>
                        <span class="section-label">${section.label}</span>
                      </div>
                      ${section.totalFormatted ? html`
                        <span class="section-pill">
                          ${section.totalFormatted}
                        </span>
                      ` : ''}
                    </div>

                    ${this.isSectionExpanded_(item.pid, section.id) ? html`
                      <table class="sub-table">
                        <thead>
                          <tr>
                            <th class="sub-th-label">Metric</th>
                            <th class="sub-th-val num-col">Value</th>
                            <th class="magnitude-col">Magnitude</th>
                            <th class="sub-th-trend trend-col">Trend (20s)</th>
                          </tr>
                        </thead>
                        <tbody>
                          ${section.metrics.map(metric => html`
                            <tr class="sub-table-row">
                              <td class="sub-td-label">${metric.label}</td>
                              <td class="sub-td-val num-col">
                                ${metric.valueFormatted}
                              </td>
                              <td class="magnitude-col">
                                ${metric.magnitudePercent !== null ? html`
                                  <div class="magnitude-bar-track">
                                    <div class="magnitude-bar-fill"
                                         style="width: ${
                                             metric.magnitudePercent}%;">
                                    </div>
                                  </div>
                                ` : ''}
                              </td>
                              <td class="sub-td-trend trend-col">
                                <div class="trend-sparkline">
                                  ${this.getSparklineBars(metric.history, metric.maxScale)
                                      .map(bar => html`
                                    <div class="sparkline-bar ${
                                        bar.isEmpty ? 'empty' : ''} ${
                                        bar.isError ? 'error' : ''}"
                                         style="height: ${bar.height}px;"
                                         title="${bar.isEmpty ? '' :
                                             (bar.isError ?
                                              'Memory dump unavailable' :
                                              (bar.value !== null ?
                                               bar.value.toLocaleString() : ''))}">
                                    </div>
                                  `)}
                                </div>
                              </td>
                            </tr>
                          `)}
                        </tbody>
                      </table>
                    ` : ''}
                  </div>
                `)}
              </div>
            </td>
          </tr>
        ` : ''}
      `)}
    </tbody>
  </table>
</div>
<!--_html_template_end_-->`;
}
