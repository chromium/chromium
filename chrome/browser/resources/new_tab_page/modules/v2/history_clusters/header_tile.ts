// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../module_header.js';
import '../icons.html.js';
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {I18nMixin, loadTimeData} from '../../../i18n_setup.js';
import {MenuItem, ModuleHeaderElementV2} from '../module_header.js';

import {getTemplate} from './header_tile.html.js';

/** Element that displays a header inside a module. */
const ElementBase = I18nMixin(PolymerElement);
export class HistoryClustersHeaderElementV2 extends ElementBase {
  static get is() {
    return 'history-clusters-header-v2';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      clusterLabel: String,

      /** Whether suggestion chip header style will show. */
      suggestionChipHeaderEnabled_: {
        type: Boolean,
        reflectToAttribute: true,
        value: () => loadTimeData.getBoolean(
            'historyClustersSuggestionChipHeaderEnabled'),
      },
    };
  }

  clusterId: number;
  clusterLabel: string;
  normalizedUrl: Url;

  private onClick_(e: Event) {
    e.stopPropagation();
    this.dispatchEvent(new CustomEvent(
        'show-all-button-click', {bubbles: true, composed: true}));
  }

  private onSuggestClick_(e: Event) {
    e.stopPropagation();
    this.dispatchEvent(
        new CustomEvent('suggest-click', {bubbles: true, composed: true}));
  }

  private onMenuButtonClick_(e: Event) {
    e.stopPropagation();
    const moduleHeader = this.shadowRoot!.querySelector<ModuleHeaderElementV2>(
        'ntp-module-header-v2')!;
    moduleHeader.showAt(e);
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
              '', 'modulesDisableButtonTextV2', 'modulesThisTypeOfCardText'),
        },
        {
          action: 'show-all',
          icon: 'modules:dock_to_left',
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

customElements.define(
    HistoryClustersHeaderElementV2.is, HistoryClustersHeaderElementV2);
