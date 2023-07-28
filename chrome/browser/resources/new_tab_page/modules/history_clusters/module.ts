// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../module_header.js';
import './suggest_tile.js';

import {CrLazyRenderElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {listenOnce} from 'chrome://resources/js/util_ts.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Cart} from '../../cart.mojom-webui.js';
import {Cluster, URLVisit} from '../../history_cluster_types.mojom-webui.js';
import {LayoutType} from '../../history_clusters.mojom-webui.js';
import {I18nMixin, loadTimeData} from '../../i18n_setup.js';
import {NewTabPageProxy} from '../../new_tab_page_proxy.js';
import {InfoDialogElement} from '../info_dialog';
import {ModuleDescriptor} from '../module_descriptor.js';

import {HistoryClustersProxyImpl} from './history_clusters_proxy.js';
import {getTemplate} from './module.html.js';
import {TileModuleElement} from './tile.js';

export const LAYOUT_1_MIN_IMAGE_VISITS = 2;
export const LAYOUT_1_MIN_VISITS = 2;
export const LAYOUT_2_MIN_IMAGE_VISITS = 1;
export const LAYOUT_2_MIN_VISITS = 3;
export const LAYOUT_3_MIN_IMAGE_VISITS = 2;
export const LAYOUT_3_MIN_VISITS = 4;

/**
 * Available module element types. This enum must match the numbering for
 * NTPHistoryClustersElementType in enums.xml. These values are persisted to
 * logs. Entries should not be renumbered, removed or reused.
 */
export enum HistoryClusterElementType {
  VISIT = 0,
  SUGGEST = 1,
  SHOW_ALL = 2,
  CART = 3,
  OPEN_ALL = 4,
}

/**
 * The overall image presence state of the visit tiles on unloading the page.
 * This enum must match the numbering for NTPHistoryClustersImageDisplayState in
 * enums.xml. These values are persisted to logs. Entries should not be
 * renumbered, removed or reused.
 */
export enum HistoryClusterImageDisplayState {
  NONE = 0,
  SOME = 1,
  ALL = 2,
}

export interface HistoryClustersModuleElement {
  $: {
    infoDialogRender: CrLazyRenderElement<InfoDialogElement>,
  };
}

export class HistoryClustersModuleElement extends I18nMixin
(PolymerElement) {
  static get is() {
    return 'ntp-history-clusters';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      layoutType: Number,

      /** The cluster displayed by this element. */
      cluster: Object,

      /** The cart displayed by this element, could be null. */
      cart: {
        type: Object,
        value: null,
      },

      searchResultPage: Object,

      overflowScroll_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('modulesOverflowScrollbarEnabled'),
        reflectToAttribute: true,
      },
    };
  }

  cluster: Cluster;
  cart: Cart|null;
  layoutType: LayoutType;
  searchResultPage: URLVisit;
  private setDisabledModulesListenerId_: number|null = null;

  override ready() {
    super.ready();

    HistoryClustersProxyImpl.getInstance().handler.recordLayoutTypeShown(
        this.layoutType, this.cluster.id);

    listenOnce(window, 'unload', () => {
      const visitTiles: TileModuleElement[] = Array.from(
          this.shadowRoot!.querySelectorAll('ntp-history-clusters-tile'));
      const count = visitTiles.reduce(
          (acc, tile) => acc + (tile.hasImageUrl() ? 1 : 0), 0);
      const state = (visitTiles.length === count) ?
          HistoryClusterImageDisplayState.ALL :
          (count === 0) ? HistoryClusterImageDisplayState.NONE :
                          HistoryClusterImageDisplayState.SOME;
      chrome.metricsPrivate.recordEnumerationValue(
          `NewTabPage.HistoryClusters.Layout${
              this.layoutType}.ImageDisplayState`,
          state, Object.keys(HistoryClusterImageDisplayState).length);
    });
  }

  override connectedCallback() {
    super.connectedCallback();

    if (loadTimeData.getBoolean(
            'modulesChromeCartInHistoryClustersModuleEnabled')) {
      this.setDisabledModulesListenerId_ =
          NewTabPageProxy.getInstance()
              .callbackRouter.setDisabledModules.addListener(
                  async (_: boolean, ids: string[]) => {
                    if (ids.includes('chrome_cart')) {
                      this.cart = null;
                    } else if (!this.cart) {
                      const {cart} =
                          await HistoryClustersProxyImpl.getInstance()
                              .handler.getCartForCluster(this.cluster);
                      this.cart = cart;
                    }
                  });
    }
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    if (this.setDisabledModulesListenerId_) {
      NewTabPageProxy.getInstance().callbackRouter.removeListener(
          this.setDisabledModulesListenerId_);
    }
  }

  private isLayout_(type: LayoutType): boolean {
    return type === this.layoutType;
  }

  private onVisitTileClick_(e: Event) {
    this.recordTileClickIndex_(e.target as HTMLElement, 'Visit');
    this.recordClick_(HistoryClusterElementType.VISIT);
  }

  private onSuggestTileClick_(e: Event) {
    this.recordTileClickIndex_(e.target as HTMLElement, 'Suggest');
    this.recordClick_(HistoryClusterElementType.SUGGEST);
  }

  private onCartTileClick_() {
    this.recordClick_(HistoryClusterElementType.CART);
  }

  private recordClick_(type: HistoryClusterElementType) {
    chrome.metricsPrivate.recordEnumerationValue(
        `NewTabPage.HistoryClusters.Layout${this.layoutType}.Click`, type,
        Object.keys(HistoryClusterElementType).length);
    HistoryClustersProxyImpl.getInstance().handler.recordClick(this.cluster.id);

    this.dispatchEvent(new Event('usage', {bubbles: true, composed: true}));
  }

  private recordTileClickIndex_(tile: HTMLElement, tileType: string) {
    assert(this.layoutType !== LayoutType.kNone);
    const index = Array.from(tile.parentNode!.children).indexOf(tile);
    chrome.metricsPrivate.recordValue(
        {
          metricName: `NewTabPage.HistoryClusters.Layout${this.layoutType}.${
              tileType!}Tile.ClickIndex`,
          type: chrome.metricsPrivate.MetricTypeType.HISTOGRAM_LINEAR,
          min: 0,
          max: 10,
          buckets: 10,
        },
        index);
  }

  private onDisableButtonClick_() {
    HistoryClustersProxyImpl.getInstance().handler.recordDisabled(
        this.cluster.id);
    const disableEvent = new CustomEvent('disable-module', {
      composed: true,
      detail: {
        message: loadTimeData.getStringF(
            'disableModuleToastMessage',
            loadTimeData.getString('modulesJourneysSentence2')),
      },
    });
    this.dispatchEvent(disableEvent);
  }

  private onDismissButtonClick_() {
    HistoryClustersProxyImpl.getInstance().handler.dismissCluster(
        [this.searchResultPage, ...this.cluster.visits], this.cluster.id);
    this.dispatchEvent(new CustomEvent('dismiss-module', {
      bubbles: true,
      composed: true,
      detail: {
        message: loadTimeData.getStringF(
            'dismissModuleToastMessage', this.cluster.label),
      },
    }));
  }

  private onInfoButtonClick_() {
    this.$.infoDialogRender.get().showModal();
  }

  private onShowAllClick_() {
    assert(this.cluster.label.length >= 2);
    HistoryClustersProxyImpl.getInstance().handler.showJourneysSidePanel(
        this.cluster.label.substring(1, this.cluster.label.length - 1));
    this.recordClick_(HistoryClusterElementType.SHOW_ALL);
  }

  private onOpenAllInTabGroupClick_() {
    const urls = [this.searchResultPage, ...this.cluster.visits].map(
        visit => visit.normalizedUrl);
    HistoryClustersProxyImpl.getInstance().handler.openUrlsInTabGroup(
        urls, this.cluster.tabGroupName ?? null);
    this.recordClick_(HistoryClusterElementType.OPEN_ALL);
  }

  private shouldShowCartTile_(cart: Object): boolean {
    return loadTimeData.getBoolean(
               'modulesChromeCartInHistoryClustersModuleEnabled') &&
        !!cart;
  }
}

customElements.define(
    HistoryClustersModuleElement.is, HistoryClustersModuleElement);

function recordSelectedLayout(option: LayoutType) {
  chrome.metricsPrivate.recordEnumerationValue(
      'NewTabPage.HistoryClusters.DisplayLayout', option,
      Object.keys(LayoutType).length);
}

function processLayoutVisits(
    visits: URLVisit[], numVisits: number, numImageVisits: number): URLVisit[] {
  const result: URLVisit[] = Array<URLVisit>(numVisits);
  let currentImageIdx = 0;
  let currentVisitIdx = numImageVisits;
  for (let i = 0; i < visits.length; i++) {
    if (currentImageIdx < numImageVisits && visits[i].hasUrlKeyedImage) {
      result[currentImageIdx] = visits[i];
      currentImageIdx++;
    } else if (currentVisitIdx < numVisits) {
      result[currentVisitIdx] = visits[i];
      currentVisitIdx++;
    } else {
      break;
    }
  }
  return result;
}

async function createElement(): Promise<HistoryClustersModuleElement|null> {
  const {clusters} =
      await HistoryClustersProxyImpl.getInstance().handler.getClusters();
  // Do not show module if there are no clusters.
  if (clusters.length === 0) {
    recordSelectedLayout(LayoutType.kNone);
    return null;
  }

  const element = new HistoryClustersModuleElement();
  element.cluster = clusters[0];
  // Initialize the cart element when the feature is enabled.
  if (loadTimeData.getBoolean(
          'modulesChromeCartInHistoryClustersModuleEnabled')) {
    const {cart} =
        await HistoryClustersProxyImpl.getInstance().handler.getCartForCluster(
            clusters[0]);
    element.cart = cart;
  }
  // Pull out the SRP to be used in the header and to open the cluster
  // in tab group.
  element.searchResultPage = clusters[0]!.visits[0];

  // History cluster visits minus the SRP that is included, since the SRP
  // isn't used in the layout.
  const visits = element.cluster.visits.slice(1);
  // Count number of visits with images.
  const imageCount = visits
                         .filter(
                             (visit: URLVisit) =>
                                 visit.hasUrlKeyedImage && visit.isKnownToSync)
                         .length;
  const visitCount = visits.length;

  // Calculate which layout to use.
  if (imageCount >= LAYOUT_3_MIN_IMAGE_VISITS) {
    // Layout 1 and 3 require the same number of images.
    // Decide which one to use by checking if there are enough total
    // visits for layout 3.
    if (visitCount >= LAYOUT_3_MIN_VISITS) {
      element.layoutType = LayoutType.kLayout3;
      element.cluster.visits = processLayoutVisits(
          visits, LAYOUT_3_MIN_VISITS, LAYOUT_3_MIN_IMAGE_VISITS);
    } else {
      // If we have enough image visits, we have enough total visits
      // for layout 1, since all visits shown are image visits.
      element.layoutType = LayoutType.kLayout1;
      element.cluster.visits = processLayoutVisits(
          visits, LAYOUT_1_MIN_VISITS, LAYOUT_1_MIN_IMAGE_VISITS);
    }
  } else if (
      imageCount === LAYOUT_2_MIN_IMAGE_VISITS &&
      visitCount >= LAYOUT_2_MIN_VISITS) {
    element.layoutType = LayoutType.kLayout2;
    element.cluster.visits = processLayoutVisits(
        visits, LAYOUT_2_MIN_VISITS, LAYOUT_2_MIN_IMAGE_VISITS);
  } else {
    // If the data doesn't fit any layout, don't show the module.
    recordSelectedLayout(LayoutType.kNone);
    return null;
  }

  recordSelectedLayout(element.layoutType);
  return element;
}

export const historyClustersDescriptor: ModuleDescriptor = new ModuleDescriptor(
    /*id=*/ 'history_clusters', createElement);
