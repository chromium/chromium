// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../module_header.js';
import './suggest_tile.js';
import './tile.js';

import {CrLazyRenderElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Cluster, URLVisit} from '../../history_cluster_types.mojom-webui.js';
import {I18nMixin, loadTimeData} from '../../i18n_setup.js';
import {InfoDialogElement} from '../info_dialog';
import {ModuleDescriptor} from '../module_descriptor.js';

import {HistoryClustersProxyImpl} from './history_clusters_proxy.js';
import {getTemplate} from './module.html.js';

export const LAYOUT_1_MIN_IMAGE_VISITS = 2;
export const LAYOUT_1_MIN_VISITS = 2;
export const LAYOUT_2_MIN_IMAGE_VISITS = 1;
export const LAYOUT_2_MIN_VISITS = 3;
export const LAYOUT_3_MIN_IMAGE_VISITS = 2;
export const LAYOUT_3_MIN_VISITS = 4;
export const MIN_RELATED_SEARCHES = 3;

/**
 * Available module UI layouts. This enum must match the numbering for
 * NTPHistoryClustersModuleDisplayLayout in enums.xml. These values are
 * persisted to logs. Entries should not be renumbered, removed or reused.
 */
export enum HistoryClusterLayoutType {
  NONE = 0,
  LAYOUT_1 = 1,  // 2 image visits
  LAYOUT_2 = 2,  // 1 image visit & 2 non-image visits
  LAYOUT_3 = 3,  // 2 image visits & 2 non-image visits
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

      searchResultPage: Object,
    };
  }

  cluster: Cluster;
  layoutType: HistoryClusterLayoutType;
  searchResultPage: URLVisit;

  private isLayout_(type: HistoryClusterLayoutType): boolean {
    return type === this.layoutType;
  }

  private onVisitTileClick_(e: Event) {
    this.recordClick_(e.target as HTMLElement, 'Visit');
  }

  private onSuggestTileClick_(e: Event) {
    this.recordClick_(e.target as HTMLElement, 'Suggest');
  }

  private recordClick_(tile: HTMLElement, tileType: string) {
    assert(this.layoutType !== HistoryClusterLayoutType.NONE);
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

    this.dispatchEvent(new Event('usage', {bubbles: true, composed: true}));
  }

  private onDisableButtonClick_() {
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
        [this.searchResultPage, ...this.cluster.visits]);
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
  }
}

customElements.define(
    HistoryClustersModuleElement.is, HistoryClustersModuleElement);

function recordSelectedLayout(option: HistoryClusterLayoutType) {
  chrome.metricsPrivate.recordEnumerationValue(
      'NewTabPage.HistoryClusters.DisplayLayout', option,
      Object.keys(HistoryClusterLayoutType).length);
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
  const data =
      await HistoryClustersProxyImpl.getInstance().handler.getCluster();
  // Do not show module if no cluster or not enough related search results.
  if (!data.cluster ||
      data.cluster.relatedSearches.length < MIN_RELATED_SEARCHES) {
    recordSelectedLayout(HistoryClusterLayoutType.NONE);
    return null;
  }

  const element = new HistoryClustersModuleElement();
  element.cluster = data.cluster!;
  // Pull out the SRP to be used in the header and to open the cluster
  // in tab group.
  element.searchResultPage = data.cluster!.visits[0];

  // History cluster visits minus the SRP that is included, since the SRP
  // isn't used in the layout.
  const visits = element.cluster.visits.slice(1);
  // Count number of visits with images.
  const imageCount =
      visits.filter((visit: URLVisit) => visit.hasUrlKeyedImage).length;
  const visitCount = visits.length;

  // Calculate which layout to use.
  if (imageCount >= LAYOUT_3_MIN_IMAGE_VISITS) {
    // Layout 1 and 3 require the same number of images.
    // Decide which one to use by checking if there are enough total
    // visits for layout 3.
    if (visitCount >= LAYOUT_3_MIN_VISITS) {
      element.layoutType = HistoryClusterLayoutType.LAYOUT_3;
      element.cluster.visits = processLayoutVisits(
          visits, LAYOUT_3_MIN_VISITS, LAYOUT_3_MIN_IMAGE_VISITS);
    } else {
      // If we have enough image visits, we have enough total visits
      // for layout 1, since all visits shown are image visits.
      element.layoutType = HistoryClusterLayoutType.LAYOUT_1;
      element.cluster.visits = processLayoutVisits(
          visits, LAYOUT_1_MIN_VISITS, LAYOUT_1_MIN_IMAGE_VISITS);
    }
  } else if (
      imageCount === LAYOUT_2_MIN_IMAGE_VISITS &&
      visitCount >= LAYOUT_2_MIN_VISITS) {
    element.layoutType = HistoryClusterLayoutType.LAYOUT_2;
    element.cluster.visits = processLayoutVisits(
        visits, LAYOUT_2_MIN_VISITS, LAYOUT_2_MIN_IMAGE_VISITS);
  } else {
    // If the data doesn't fit any layout, don't show the module.
    recordSelectedLayout(HistoryClusterLayoutType.NONE);
    return null;
  }

  recordSelectedLayout(element.layoutType);
  return element;
}

export const historyClustersDescriptor: ModuleDescriptor = new ModuleDescriptor(
    /*id=*/ 'history_clusters', createElement);
