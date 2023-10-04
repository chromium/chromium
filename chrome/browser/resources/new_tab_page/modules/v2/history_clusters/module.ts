// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './cart/cart_tile.js';
import './header_tile.js';
import './suggest_tile.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import '../../../discount.mojom-webui.js';

import {CrLazyRenderElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Cart} from '../../../cart.mojom-webui.js';
import {Cluster, InteractionState} from '../../../history_cluster_types.mojom-webui.js';
import {LayoutType} from '../../../history_clusters_layout_type.mojom-webui.js';
import {I18nMixin, loadTimeData} from '../../../i18n_setup.js';
import {NewTabPageProxy} from '../../../new_tab_page_proxy.js';
import {InfoDialogElement} from '../../info_dialog';
import {ModuleDescriptor} from '../../module_descriptor.js';

import {HistoryClustersProxyImpl} from './history_clusters_proxy.js';
import {getTemplate} from './module.html.js';
import {VisitTileModuleElement} from './visit_tile.js';

export const MAX_MODULE_ELEMENT_INSTANCES = 3;

const CLUSTER_MIN_REQUIRED_URL_VISITS = 3;

export interface HistoryClustersModuleElement {
  $: {
    infoDialogRender: CrLazyRenderElement<InfoDialogElement>,
  };
}

export class HistoryClustersModuleElement extends I18nMixin
(PolymerElement) {
  static get is() {
    return 'ntp-history-clusters-redesigned';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      layoutType: Number,

      /** The cart displayed by this element, could be null. */
      cart: {
        type: Object,
        value: null,
      },

      /**
        The discounts displayed on the visit tiles of this element, could be
        empty.
      */
      discounts: {
        type: Array,
        value: [],
      },

      /** The cluster displayed by this element. */
      cluster: {
        type: Object,
      },

      format: {
        type: String,
        reflectToAttribute: true,
      },

      imagesEnabled_: {
        type: Boolean,
        reflectToAttribute: true,
        value: () => loadTimeData.getBoolean('historyClustersImagesEnabled'),
      },

      showRelatedSearches: {
        type: Boolean,
        computed: `computeShowRelatedSearches(cluster)`,
        reflectToAttribute: true,
      },
    };
  }

  cart: Cart|null;
  discounts: string[];
  cluster: Cluster;
  format: string;
  showRelatedSearches: boolean;
  private imagesEnabled_: boolean;
  private setDisabledModulesListenerId_: number|null = null;

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

  override ready() {
    super.ready();

    HistoryClustersProxyImpl.getInstance().handler.recordLayoutTypeShown(
        this.imagesEnabled_ ? LayoutType.kImages : LayoutType.kTextOnly,
        this.cluster.id);
  }

  private computeShowRelatedSearches(): boolean {
    return this.cluster.relatedSearches.length > 1;
  }

  private computeUnquotedClusterLabel_(): string {
    return this.cluster.label.substring(1, this.cluster.label.length - 1);
  }

  private shouldShowCartTile_(cart: Object): boolean {
    return loadTimeData.getBoolean(
               'modulesChromeCartInHistoryClustersModuleEnabled') &&
        !!cart;
  }

  private onDisableButtonClick_() {
    const disableEvent = new CustomEvent('disable-module', {
      composed: true,
      detail: {
        message: loadTimeData.getStringF(
            'modulesDisableToastMessage',
            loadTimeData.getString('modulesThisTypeOfCardText')),
      },
    });
    this.dispatchEvent(disableEvent);
  }

  private onDismissButtonClick_() {
    HistoryClustersProxyImpl.getInstance()
        .handler.updateClusterVisitsInteractionState(
            this.cluster.visits, InteractionState.kHidden);
    this.dispatchEvent(new CustomEvent('dismiss-module-instance', {
      bubbles: true,
      composed: true,
      detail: {
        message: loadTimeData.getStringF(
            'dismissModuleToastMessage', this.cluster.label),
        restoreCallback: () => {
          HistoryClustersProxyImpl.getInstance()
              .handler.updateClusterVisitsInteractionState(
                  this.cluster.visits, InteractionState.kDefault);
        },
      },
    }));
  }

  private onDoneButtonClick_() {
    HistoryClustersProxyImpl.getInstance()
        .handler.updateClusterVisitsInteractionState(
            this.cluster.visits, InteractionState.kDone);
    this.dispatchEvent(new CustomEvent('dismiss-module-instance', {
      bubbles: true,
      composed: true,
      detail: {
        message: loadTimeData.getStringF(
            'dismissModuleToastMessage', this.cluster.label),
        restoreCallback: () => {
          HistoryClustersProxyImpl.getInstance()
              .handler.updateClusterVisitsInteractionState(
                  this.cluster.visits, InteractionState.kDefault);
        },
      },
    }));
  }

  private onInfoButtonClick_() {
    this.$.infoDialogRender.get().showModal();
  }

  private onShowAllButtonClick_() {
    assert(this.cluster.label.length >= 2, 'Unexpected cluster label length');
    HistoryClustersProxyImpl.getInstance().handler.showJourneysSidePanel(
        this.computeUnquotedClusterLabel_());
    this.dispatchEvent(new Event('usage', {bubbles: true, composed: true}));
  }

  private onSuggestTileClick_(e: Event) {
    this.recordTileClickIndex_(e.target as HTMLElement, 'Suggest');
    this.recordClick_();
  }

  private onVisitTileClick_(e: Event) {
    this.recordTileClickIndex_(e.target as HTMLElement, 'Visit');
    this.recordClick_();
    this.maybeRecordDiscountClick_(e.target as VisitTileModuleElement);
  }

  private recordTileClickIndex_(tile: HTMLElement, tileType: string) {
    const layoutType =
        this.imagesEnabled_ ? LayoutType.kImages : LayoutType.kTextOnly;
    const index = Array.from(tile.parentNode!.children).indexOf(tile);
    chrome.metricsPrivate.recordValue(
        {
          metricName: `NewTabPage.HistoryClusters.Layout${layoutType}.${
              tileType!}Tile.ClickIndex`,
          type: chrome.metricsPrivate.MetricTypeType.HISTOGRAM_LINEAR,
          min: 0,
          max: 10,
          buckets: 10,
        },
        index);
  }

  private recordClick_() {
    HistoryClustersProxyImpl.getInstance().handler.recordClick(this.cluster.id);
    this.dispatchEvent(new Event('usage', {bubbles: true, composed: true}));
  }

  private maybeRecordDiscountClick_(tile: VisitTileModuleElement) {
    if (tile.hasDiscount) {
      chrome.metricsPrivate.recordUserAction(
          `NewTabPage.HistoryClusters.DiscountClicked`);
    }
  }

  private getInfo_(discounts: string[]): TrustedHTML {
    const hasDiscount = discounts.some((discount) => !!discount);
    return this.i18nAdvanced(
        hasDiscount ? 'modulesHistoryWithDiscountInfo' :
                      'modulesJourneysInfo');
  }
}

customElements.define(
    HistoryClustersModuleElement.is, HistoryClustersModuleElement);

async function createElement(cluster: Cluster):
    Promise<HistoryClustersModuleElement> {
  const element = new HistoryClustersModuleElement();
  element.cluster = cluster;
  if (loadTimeData.getBoolean(
          'modulesChromeCartInHistoryClustersModuleEnabled')) {
    const {cart} =
        await HistoryClustersProxyImpl.getInstance().handler.getCartForCluster(
            cluster);
    element.cart = cart;
  }

  element.discounts = [];
  if (loadTimeData.getBoolean('historyClustersModuleDiscountsEnabled')) {
    const {discounts} = await HistoryClustersProxyImpl.getInstance()
                            .handler.getDiscountsForCluster(cluster);
    for (const visit of cluster.visits) {
      let discountInValue = '';
      for (const [url, urlDiscounts] of discounts) {
        if (url.url === visit.normalizedUrl.url && urlDiscounts.length > 0) {
          // API is designed to support multiple discounts, but for now we only
          // have one.
          discountInValue = urlDiscounts[0].valueInText;
          visit.normalizedUrl.url = urlDiscounts[0].annotatedVisitUrl.url;
        }
      }
      element.discounts.push(discountInValue);
    }
    // For visits without discounts, discount string in corresponding index in
    // `discounts` array is empty.
    // Only interested in the discounts for the first two visits (first three
    // elements in the array) since they are the only visible ones.
    const hasDiscount =
        element.discounts.slice(0, CLUSTER_MIN_REQUIRED_URL_VISITS)
            .some((discount) => discount.length > 0);
    chrome.metricsPrivate.recordBoolean(
        `NewTabPage.HistoryClusters.HasDiscount`, hasDiscount);
  } else {
    element.discounts = Array(cluster.visits.length).fill('');
  }
  return element;
}

async function createElements(): Promise<HTMLElement[]|null> {
  const {clusters} =
      await HistoryClustersProxyImpl.getInstance().handler.getClusters();
  if (!clusters || clusters.length === 0) {
    return null;
  }

  const elements: HistoryClustersModuleElement[] = [];
  for (let i = 0; i < clusters.length; i++) {
    if (elements.length === MAX_MODULE_ELEMENT_INSTANCES) {
      break;
    }
    if (clusters[i].visits.length >= CLUSTER_MIN_REQUIRED_URL_VISITS) {
      elements.push(await createElement(clusters[i]));
    }
  }

  return (elements as unknown) as HTMLElement[];
}

export const historyClustersDescriptor: ModuleDescriptor = new ModuleDescriptor(
    /*id=*/ 'history_clusters', createElements);
