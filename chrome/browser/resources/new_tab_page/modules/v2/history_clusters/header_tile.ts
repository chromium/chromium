// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../module_header.js';
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';

import {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {I18nMixin, loadTimeData} from '../../../i18n_setup.js';

import {getTemplate} from './header_tile.html.js';

export interface HistoryClustersHeaderElementV2 {
  $: {
    actionMenu: CrActionMenuElement,
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
      dismissText: String,
      disableText: String,

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
  dismissText: string;
  disableText: string;
  private suggestionChipHeaderEnabled_: boolean;

  private onButtonClick_(e: Event) {
    const action: string = (e.target! as any).dataset.action;
    assert(action);
    this.$.actionMenu.close();
    this.dispatchEvent(
        new Event(`${action}-button-click`, {bubbles: true, composed: true}));
  }

  private onMenuButtonClick_(e: Event) {
    this.$.actionMenu.showAt(e.target as HTMLElement);
  }

  private onCustomizeButtonClick_() {
    this.$.actionMenu.close();
    this.dispatchEvent(
        new Event('customize-module', {bubbles: true, composed: true}));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'history-clusters-header-v2': HistoryClustersHeaderElementV2;
  }
}

customElements.define(
    HistoryClustersHeaderElementV2.is, HistoryClustersHeaderElementV2);
