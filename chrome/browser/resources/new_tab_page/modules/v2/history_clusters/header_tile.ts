// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../module_header.js';
import '../icons.html.js';
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {I18nMixin, loadTimeData} from '../../../i18n_setup.js';
import {MenuItem, ModuleHeaderElementV2} from '../module_header.js';

import {getTemplate} from './header_tile.html.js';

export interface HistoryClustersHeaderElementV2 {
  $: {
    moduleHeaderElementV2: ModuleHeaderElementV2,
  };
}

/** Element that displays a header inside a module. */
export class HistoryClustersHeaderElementV2 extends I18nMixin
(PolymerElement) {
  static get is() {
    return 'history-clusters-header-v2';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      clusterLabel: String,

      /** Whether suggestion chip header will show. */
      suggestionChipHeaderEnabled_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean(
            'historyClustersSuggestionChipHeaderEnabled'),
        reflectToAttribute: true,
      },
    };
  }

  clusterLabel: string;
  private suggestionChipHeaderEnabled_: boolean;

  private onMenuButtonClick_(e: Event) {
    this.$.moduleHeaderElementV2.showAt(e);
  }

  private getMenuItemGroups_(): MenuItem[][] {
    return [
      [
        {
          action: 'done',
          icon: 'modules:done',
          text: this.i18n('modulesJourneysDoneButton'),
        },
        {
          action: 'dismiss',
          icon: 'modules:thumb_down',
          text: this.i18n('modulesJourneysDismissButton'),
        },
        {
          action: 'disable',
          icon: 'modules:block',
          text: this.i18nRecursive(
              '', 'modulesDisableButtonTextV2', 'modulesJourneyDisable'),
        },
        {
          action: 'show-all',
          icon: 'modules:right_panel_open',
          text: this.i18n('modulesJourneysShowAllButton'),
        },
        {
          action: 'info',
          icon: 'modules:info',
          text: this.i18n('moduleInfoButtonTitle'),
        },
      ],
      [
        {
          action: 'customize-module',
          icon: 'modules:tune',
          text: this.i18n('modulesCustomizeButtonText'),
        },
      ],
    ];
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'history-clusters-header-v2': HistoryClustersHeaderElementV2;
  }
}

customElements.define(
    HistoryClustersHeaderElementV2.is, HistoryClustersHeaderElementV2);
