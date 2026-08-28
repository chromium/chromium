// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {LoadingPageElement} from './loading_page.js';

export function getHtml(this: LoadingPageElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
    <div role="status" aria-busy="true">
      ${this.dialog ? html`
        <div class="dialog-layout">
          <div class="dialog-container">
            <div class="skeleton-dialog-title"></div>
            <div class="skeleton-dialog-subtitle">
              <div class="skeleton-line full"></div>
              <div class="skeleton-line half"></div>
            </div>

            <div class="dialog-field-group">
              <div class="skeleton-dialog-label"></div>
              <div class="skeleton-dialog-input">
                <div class="skeleton-dialog-icon"></div>
                <div class="skeleton-line short"></div>
              </div>
            </div>

            <div class="dialog-field-group">
              <div class="skeleton-dialog-label wide"></div>
              <div class="skeleton-dialog-input">
                <div class="skeleton-line medium"></div>
              </div>
            </div>

            <div class="dialog-field-group">
              <div class="skeleton-dialog-label"></div>
              <div class="skeleton-dialog-textarea">
                <div class="skeleton-line full"></div>
                <div class="skeleton-line medium"></div>
                <div class="skeleton-line short"></div>
                <div class="skeleton-dialog-toolbar">
                  <div class="skeleton-dialog-icon-small"></div>
                  <div class="skeleton-dialog-icon-small"></div>
                  <div class="skeleton-dialog-icon-small"></div>
                </div>
              </div>
            </div>

            <div class="dialog-actions-row">
              <div class="skeleton-dialog-btn open-full-page"></div>
              <div class="dialog-actions-right">
                <div class="skeleton-dialog-btn"></div>
                <div class="skeleton-dialog-btn"></div>
              </div>
            </div>
          </div>
          <div class="dialog-footer-note">
            <div class="skeleton-line medium"></div>
          </div>
        </div>
      ` : html`
        <div class="app-layout">
          <div class="sidebar-container">
            <div class="sidebar-nav">
              <div class="skeleton-nav-item"></div>
              <div class="skeleton-nav-item"></div>
              <div class="skeleton-nav-item"></div>
              <div class="skeleton-nav-item"></div>
            </div>
          </div>
          <div class="main-content">
            ${this.editor ? html`
              <div class="editor-container">
                <div class="header-actions">
                  <div class="skeleton-search"></div>
                  <div class="skeleton-btn save-btn"></div>
                </div>

                <div class="skeleton-editor-desc"></div>

                <div class="editor-metadata-row">
                  <div class="skeleton-file-info"></div>
                </div>

                <div class="skeleton-editor-card"></div>
              </div>
            ` : html`
              <div class="browse-skills-container">
                <div class="header-actions">
                  <div class="skeleton-search"></div>
                </div>
                <section class="skills-section">
                  <div class="skeleton-section-title"></div>
                  <div class="skills-grid">
                    <div class="skeleton-card"></div>
                    <div class="skeleton-card"></div>
                    <div class="skeleton-card"></div>
                  </div>
                </section>
                <section class="skills-section">
                  <div class="skeleton-section-title"></div>
                  <div class="skeleton-chips">
                    <div class="skeleton-chip"></div>
                    <div class="skeleton-chip"></div>
                    <div class="skeleton-chip"></div>
                    <div class="skeleton-chip"></div>
                    <div class="skeleton-chip"></div>
                    <div class="skeleton-chip"></div>
                    <div class="skeleton-chip"></div>
                  </div>
                  <div class="skills-grid">
                    <div class="skeleton-card"></div>
                    <div class="skeleton-card"></div>
                    <div class="skeleton-card"></div>
                  </div>
                </section>
              </div>
            `}
          </div>
        </div>
      `}
    </div>
  <!--_html_template_end_-->`;
  // clang-format on
}
