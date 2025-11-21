// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SetupListElement} from './setup_list.js';

export function getHtml(this: SetupListElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<ntp-module-header
    id="moduleHeaderElementV2"
    header-text="${this.i18n('modulesSetupListTitle')}"
    .menuItems="${[
        {
          action: 'dismiss',
          icon: 'modules:visibility_off',
          text: this.i18nRecursive(
              '', 'modulesDismissForDaysButtonText',
              'setupListModuleDismissDays'),
        },
        {
          action: 'disable',
          icon: 'modules:block',
          text: this.i18nRecursive(
              '', 'modulesDisableButtonTextV2', 'modulesSetupListTitle'),
        },
    ]}"
    hide-customize
    more-actions-text="${this.i18nRecursive('',
                                  'modulesMoreActions',
                                  'modulesSetupListTitle')}"
    @disable-button-click="${this.onDisableButtonClick_}"
    @dismiss-button-click="${this.onDismissButtonClick_}"
    @info-button-click="${this.onInfoButtonClick_}">
</ntp-module-header>
<div id="promos" @ntp-promo-click="${this.onPromoClick_}">
  ${this.eligiblePromos_.map(item => html`
    <setup-list-item
        body-icon-name="${item.iconName}"
        body-text="${item.bodyText}"
        action-button-text="${item.buttonText}"
        promo-id="${item.id}">
    </setup-list-item>
  `)}
  ${this.completedPromos_.map(item => html`
    <setup-list-item
        completed="true"
        body-text="${item.bodyText}"
        promo-id="${item.id}">
    </setup-list-item>
  `)}
</div>
${this.showInfoDialog_ ? html`
    <ntp-info-dialog show-on-attach
        .innerHTML="${this.i18nAdvanced('modulesSetupListInfo')}"
        @close="${this.onInfoDialogClose_}">>
    </ntp-info-dialog>
` : ''}
<!--_html_template_end_-->`;
  // clang-format on
}
