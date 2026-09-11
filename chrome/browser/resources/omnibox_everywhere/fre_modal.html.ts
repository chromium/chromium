// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {FreModalElement} from './fre_modal.js';

export function getHtml(this: FreModalElement) {
  return html`
    <div class="fre-card">
      <div class="top-section">
        <div class="content">
          <div class="header">
            <img class="chrome-logo"
                src="images/product-logo.svg"
                alt="" role="presentation">
            <div class="title">${this.i18n('loomniboxFreTitle')}</div>
          </div>

          <div class="containers">
            <div class="second-container">
              ${this.isFuseboxEligible_() ? html`
                <div class="list-item">
                  <div class="icon lens-icon"></div>
                  <div class="item-text">
                    <div class="primary-text">
                      ${this.i18n('loomniboxFreLensPrimary')}
                    </div>
                    <div class="secondary-text">
                      ${this.i18n('loomniboxFreLensSecondary')}
                    </div>
                  </div>
                </div>
              ` : ''}

              ${this.isMac_() ? html`
                <div class="list-item mac-row">
                  <div class="lhs">
                    <cr-icon class="icon open-in-new-icon"
                        icon="cr:open-in-new">
                    </cr-icon>
                    <div class="primary-text">
                      ${this.i18n('loomniboxFreWhereToFindPrimary')}
                    </div>
                  </div>
                  <div class="mac-illustration">
                    <img class="mac-menubar-img"
                        src="images/mac_menu_bar.png"
                        alt="">
                  </div>
                </div>
              ` : html`
                <div class="list-item windows-row">
                  <div class="lhs">
                    <cr-icon class="icon open-in-new-icon"
                        icon="cr:open-in-new">
                    </cr-icon>
                    <div class="primary-text">
                      ${this.i18n('loomniboxFreWhereToFindPrimary')}
                    </div>
                  </div>
                  <div class="windows-illustrations">
                    <img class="windows-taskbar-img"
                        src="images/windows_taskbar_collapsed.png"
                        alt="">
                    <img class="windows-taskbar-img"
                        src="images/windows_taskbar_expanded.png"
                        alt="">
                  </div>
                </div>
              `}
            </div>
          </div>
        </div>
      </div>

      <button class="close-button"
          aria-label="${this.i18n('loomniboxFreCloseButtonAria')}"
          @click="${this.onCloseClick_}">
        <cr-icon icon="cr:close"></cr-icon>
      </button>
    </div>
  `;
}
