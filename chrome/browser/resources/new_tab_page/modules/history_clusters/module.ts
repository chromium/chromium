// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../module_header.js';
import './tile.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Cluster, URLVisit} from '../../history_cluster_types.mojom-webui.js';
import {I18nMixin} from '../../i18n_setup.js';
import {ModuleDescriptor} from '../module_descriptor.js';

import {HistoryClustersProxyImpl} from './history_clusters_proxy.js';
import {getTemplate} from './module.html.js';

export const LAYOUT_1_3_MIN_IMAGE_VISITS = 2;
export const LAYOUT_2_IMAGE_VISITS = 1;
export const LAYOUT_2_MIN_VISITS = 3;
export const LAYOUT_3_MIN_VISITS = 4;

export enum HistoryClusterLayoutType {
  LAYOUT_1 = 'layout_1',  // 2 image visits
  LAYOUT_2 = 'layout_2',  // 1 image visit & 2 non-image visits
  LAYOUT_3 = 'layout_3',  // 2 image visits & 2 non-image visits
}

// TODO:(crbug.com/1410808): Add module UI logic.
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
      layoutType: String,
      /** The cluster displayed by this element. */
      cluster: Object,
    };
  }

  cluster: Cluster;
  layoutType: HistoryClusterLayoutType;

  private isLayout_(type: HistoryClusterLayoutType): boolean {
    return type === this.layoutType;
  }

  private onShowAllClick_() {
    HistoryClustersProxyImpl.getInstance().handler.showJourneysSidePanel(
        this.cluster.label || '');
  }
}

customElements.define(
    HistoryClustersModuleElement.is, HistoryClustersModuleElement);

async function createElement(): Promise<HistoryClustersModuleElement|null> {
  const data =
      await HistoryClustersProxyImpl.getInstance().handler.getCluster();
  if (!data.cluster) {
    return null;
  }

  const element = new HistoryClustersModuleElement();
  element.cluster = data.cluster!;

  // History cluster visits include a visit entry for the SRP, which is intended
  // to be used for the module header's title and for opening the cluster in a
  // tab group.
  const visits = element.cluster.visits;
  // Count number of visits with images.
  const imageCount =
      visits.filter((visit: URLVisit) => visit.hasUrlKeyedImage).length;
  // We subtract the SRP from the  visit count to get the actual number of
  // visits that are eligible for layout selection.
  const visitCount = visits.length - 1;

  // Calculate which layout to use.
  if (imageCount >= LAYOUT_1_3_MIN_IMAGE_VISITS) {
    // Layout 1 and 3 require the same number of images.
    // Decide which one to use by checking if there are enough total
    // visits for layout 3.
    if (visitCount >= LAYOUT_3_MIN_VISITS) {
      element.layoutType = HistoryClusterLayoutType.LAYOUT_3;
    } else {
      element.layoutType = HistoryClusterLayoutType.LAYOUT_1;
    }
  } else if (imageCount === LAYOUT_2_IMAGE_VISITS &&
      visitCount >= LAYOUT_2_MIN_VISITS) {
    element.layoutType = HistoryClusterLayoutType.LAYOUT_2;
  } else {
    // If the data doesn't fit any layout, don't show the module.
    return null;
  }

  return element;
}

export const historyClustersDescriptor: ModuleDescriptor = new ModuleDescriptor(
    /*id=*/ 'history_clusters', createElement);
