// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {FreModalElement} from './fre_modal.js';

export function getHtml(this: FreModalElement) {
  return html`
    <div class="fre-card">
      <button class="close-button"
          aria-label="${this.i18n('loomniboxFreCloseButtonAria')}"
          @click="${this.onCloseClick_}">
        <cr-icon icon="cr:close"></cr-icon>
      </button>

      <div class="top-section">
        <div class="content">
          <div class="header">
            <div class="g-logo"></div>
            <div class="title">${this.i18n('loomniboxFreTitle')}</div>
          </div>

          <div class="containers">
            <div class="second-container">
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

              <div class="list-item">
                <div class="icon keyboard-icon"></div>
                <div class="item-text horizontal-text">
                  <span class="label-text">
                    ${this.i18n('loomniboxFreKeyboardPrimary')}
                  </span>

                  <div class="keys-wrapper">
                    <div class="keys-container">
                      <div class="key-badge">
                        ${this.i18n('loomniboxFreKeyboardBadgeOption')}
                      </div>
                      <div class="key-badge">
                        ${this.i18n('loomniboxFreKeyboardBadgeSpace')}
                      </div>
                    </div>
                  </div>

                  <button class="accept-hotkey-btn"
                      aria-label="${this.i18n('loomniboxFreAcceptHotkey')}"
                      @click="${this.onAcceptHotkeyClick_}">
                    <span class="accept-label">
                      ${this.i18n('loomniboxFreAcceptHotkey')}
                    </span>
                  </button>

                  <span class="or-text">
                    ${this.i18n('loomniboxFreOr')}
                  </span>
                  <a class="edit-link" href="#"
                      @click="${this.onSettingsClick_}">
                    ${this.i18n('loomniboxFreEditOwn')}
                  </a>
                </div>
              </div>
            </div>
          </div>
        </div>
      </div>
    </div>
  `;
}
