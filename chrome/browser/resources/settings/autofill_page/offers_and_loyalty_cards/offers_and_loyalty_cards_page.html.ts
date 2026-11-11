// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsOffersAndLoyaltyCardsPageElement} from './offers_and_loyalty_cards_page.js';

export function getHtml(this: SettingsOffersAndLoyaltyCardsPageElement) {
  return html`<!--_html_template_start_-->
<settings-subpage page-title="$i18n{offersAndLoyaltyCardsCardTitle}"
    class="multi-card" route-path="${this.routePath}">
</settings-subpage>
<!--_html_template_end_-->`;
}
