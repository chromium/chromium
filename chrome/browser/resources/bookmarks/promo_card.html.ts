// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {PromoCardElement} from './promo_card.js';

export function getHtml(this: PromoCardElement) {
  // TODO(crbug.com/411439295): replace fixed strings with translatable strings.
  return html`<!--_html_template_start_-->
<div id="promoCard" role="dialog">
  <picture id="image">
    <source class="banner" srcset="use_real_image.svg "
        media="(prefers-color-scheme: dark)">
    <img class="banner" alt="" src="use_real_image_svg.svg ">
  </picture>
  <div id="promoContent">
    <h2 id="title" class="label">
      Get your bookmarks and more on all your devices
    </h2>
    <div id="description" class="cr-secondary-text label">
      ${
      this.batchUploadPromoData_
          .localBookmarksCount} bookmarks and other items are saved only to this device. To use them on your other devices, save them in your Google account, ${
      this.batchUploadPromoData_.email}
    </div>
    <cr-button id="actionButton"
        class="action-button" @click="${this.onSaveToAccountClick_}">
      Save to account
    </cr-button>
  </div>
  <cr-icon-button id="closeButton" class="icon-clear no-overlap"
      @click="${this.onCloseClick_}" title="Close"
      aria-label="Aria close">
  </cr-icon-button>
</div>
<!--_html_template_end_-->`;
}
