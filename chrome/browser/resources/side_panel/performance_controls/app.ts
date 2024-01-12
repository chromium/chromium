// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://performance-side-panel.top-chrome/shared/sp_shared_style.css.js';
import './battery_saver_card.js';
import './browser_health_card.js';
import './memory_saver_card.js';
import '../strings.m.js';

import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {listenOnce} from 'chrome://resources/js/util.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';
import {PerformanceSidePanelNotification} from './performance.mojom-webui.js';
import {PerformancePageApiProxy, PerformancePageApiProxyImpl} from './performance_page_api_proxy.js';

export interface PerformanceAppElement {
  $: {
    performanceControlsContainer: HTMLElement,
  };
}

export enum CardType {
  BROWSER_HEALTH = 0,
  MEMORY_SAVER = 1,
  BATTERY_SAVER = 2,
}

function moveCardToTop(cards: CardType[], card: CardType) {
  const index = cards.indexOf(card);
  if (index >= 0) {
    cards.unshift(cards.splice(index, 1)[0]);
  }
}

export class PerformanceAppElement extends PolymerElement {
  static get is() {
    return 'performance-app';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      cards_: {
        readOnly: true,
        type: Array,
        value: () => {
          const cards = [
            CardType.BROWSER_HEALTH,
            CardType.MEMORY_SAVER,
            CardType.BATTERY_SAVER,
          ];
          const notifications = loadTimeData.getString('sidePanelNotifications')
                                    .split(',')
                                    .map(i => parseInt(i)) as number[];
          for (const notification of notifications) {
            if (notification ===
                PerformanceSidePanelNotification
                        .kMemorySaverRevisitDiscardedTab as number) {
              moveCardToTop(cards, CardType.MEMORY_SAVER);
            }
          }
          return cards;
        },
      },

      /** Mirroring the enum so that it can be used from HTML bindings. */
      cardTypeEnum_: {
        type: Object,
        value: CardType,
      },
    };
  }

  private performanceApi_: PerformancePageApiProxy =
      PerformancePageApiProxyImpl.getInstance();

  private cards_: CardType[];

  constructor() {
    super();
    ColorChangeUpdater.forDocument().start();
  }

  override connectedCallback() {
    super.connectedCallback();

    // Wait until the first rendering of the side panel before showing the
    // side panel. Best practices for side panels require that content be
    // loaded before the side panel is shown to follow Native UI standards.
    listenOnce(this.$.performanceControlsContainer, 'dom-change', () => {
      setTimeout(() => this.performanceApi_.showUi(), 0);
    });
  }

  isEqualTo(a: CardType, b: CardType) {
    return a === b;
  }
}
declare global {
  interface HTMLElementTagNameMap {
    'performance-app': PerformanceAppElement;
  }
}
customElements.define(PerformanceAppElement.is, PerformanceAppElement);
