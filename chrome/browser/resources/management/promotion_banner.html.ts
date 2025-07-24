// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ManagementUiElement} from './management_ui.js';

export function getPromotionBannerHtml(this: ManagementUiElement) {
  // clang-format off
  return html`
    ${this.shouldShowPromotion_ ? html`
      <style>
        .banner-section {
          background-color: #E8F0FE;
          padding: 24px;
        }

        #promotion-banner-section-main.banner-section h2 {
          font-size: 16px;
        }

        .banner-section-container {
          display: flex;
          flex-direction: row;
          width: 100%;
          gap: 24px;
        }

        .banner-actions-container {
          display: flex;
          width: 100%;
          gap: 16px;
          justify-content: space-between;
          flex-direction: row;
        }

        .promotion-main-text-container {
          display: flex;
          flex-direction: column;
          gap: 12px;
        }

        .promotion-text-container {
          display: flex;
          flex-direction: column;
          gap: 4px;
        }

        .blue-pill-button {
          background-color: #0B57D0; /* Replaced with direct color */
          width: 202px;
          padding: 8px 0px 8px 0px;
          text-align: center;
          line-height: 20px;
          font-size: 13px;
          font-weight: 500;
          border: none;
          color: #FFFFFF; /* Replaced with direct color */
          border-radius: 50px;
          cursor: pointer;
          text-decoration: none;
        }

        .blue-pill-button:hover {
          background: color-mix(
            in srgb,
            #FDFCFB1A, /* Replaced with direct color */
            #0B57D0    /* Replaced with direct color */
          );
        }

        .banner-title {
          color: #202124; /* Replaced with direct color */
          font-weight: 500;
          line-height: 24px;
          display: contents;
        }

        .banner-description {
          color: #202124; /* Replaced with direct color */
          font-size: 13px;
          font-weight: 400;
          line-height: 20px;
        }

        #close-icon-container {
          -webkit-mask-image: url(chrome://resources/images/promotion_policy_banner_close.svg);
          -webkit-mask-position: center;
          -webkit-mask-repeat: no-repeat;
          -webkit-mask-size: 20px;
          background-color: #5f6368;
          height: 20px;
          width: 20px;
        }

        @media (prefers-color-scheme: dark) {
          /* No more --promotion-* variables defined on html {} as they are being replaced directly */

          .banner-section {
            background-color: #3C4043; /* Replaced with direct color */
          }

          .blue-pill-button {
            background-color: #A8C7FA; /* Replaced with direct color */
            color: #062E6F; /* Replaced with direct color */
          }

          .blue-pill-button:hover {
            background: color-mix(
              in srgb,
              #1F1F1F0F, /* Replaced with direct color */
              #A8C7FA    /* Replaced with direct color */
            );
          }

          .banner-title {
            color: #FFFFFF; /* Replaced with direct color */
          }

          .banner-description {
            color: #FFFFFF; /* Replaced with direct color */
          }

          #close-icon-container {
            background-color: #f1f3f4;
          }
        }

        @media (forced-colors: active) {
          #close-icon-container {
            filter: invert(100%);
          }
        }

        .dismiss-button {
          height: 20px;
          width: 20px;
          background-color: transparent;
          cursor: pointer;
          border: none;
          padding-inline: 0;
          padding-block: 0;
        }
      </style>
      <section id="promotion-banner-section-main" class="banner-section"
          aria-label="${this.i18n('promotionBannerAriaLabel')}">
        <div id="promotion-banner-section" class="banner-section-container">
          <div id="banner-actions" class="banner-actions-container">
            <div id="promotion-main-text" class="promotion-main-text-container">
              <div id="promotion-text" class="promotion-text-container">
                <h2 id="promotion-banner-title" class="banner-title">
                  ${this.i18n('promotionBannerTitle')} </h2>
                <div id="promotion-banner-description" class="banner-description">
                  ${this.i18n('promotionBannerDesc')} </div>
              </div>
              <button id="promotion-redirect-button" class="blue-pill-button"
                  @click="${this.onPromotionRedirect_}"
                  aria-description="${this.i18n('promotionBannerNewTabAriaDescription')}">
                ${this.i18n('promotionBannerBtn')} </button>
            </div>
            <button id="promotion-dismiss-button" class="dismiss-button"
                tabindex="0" @click="${this.onDismissPromotion_}"
                aria-label="${this.i18n('promotionBannerDismissAriaLabel')}">
              <div id="close-icon-container"></div>
            </button>
          </div>
        </div>
      </section>` : ''}
  `;
  // clang-format on
}
