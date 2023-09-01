// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../module_header.js';
import '../icons.html.js';
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';
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
        reflectToAttribute: true,
        value: () => loadTimeData.getBoolean(
            'historyClustersSuggestionChipHeaderEnabled'),
      },

      /* Whether the container is tabbable or not. If the suggestion chip
       * feature is enabled, the container should not be tabbable.
       */
      containerTabIndex_: {
        type: String,
        value: () => loadTimeData.getBoolean(
                         'historyClustersSuggestionChipHeaderEnabled') ?
            '' :
            '0',
      },
    };
  }

clusterId:
  number;
clusterLabel:
  string;
normalizedUrl:
  Url;
private suggestionChipHeaderEnabled_:
  boolean;
private eventTracker_:
  EventTracker = new EventTracker();

  override connectedCallback() {
    super.connectedCallback();

    if (!this.suggestionChipHeaderEnabled_) {
      this.eventTracker_.add(this, 'click', this.onClick_);
    }
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.eventTracker_.removeAll();
  }

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
              '', 'modulesDisableButtonTextV2', 'modulesThisTypeOfCardText'),
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
