// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Cluster, URLVisit} from '../../history_cluster_types.mojom-webui.js';
import {I18nMixin} from '../../i18n_setup.js';
import {HistoryClustersProxyImpl} from '../history_clusters/history_clusters_proxy.js';
import {ModuleDescriptorV2, ModuleHeight} from '../module_descriptor.js';

import {getTemplate} from './module.html.js';

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

      /** The cluster displayed by this element. */
      cluster: {
        type: Object,
        observer: 'onClusterUpdated_',
      },

      searchResultsPage_: Object,
    };
  }

  cluster: Cluster;
  private searchResultsPage_: URLVisit;

  private onClusterUpdated_() {
    this.searchResultsPage_ = this.cluster!.visits[0];
  }
}

customElements.define(
    HistoryClustersModuleElement.is, HistoryClustersModuleElement);

async function createElement(): Promise<HTMLElement> {
  const {clusters} =
      await HistoryClustersProxyImpl.getInstance().handler.getClusters();
  // Do not show module if there are no clusters.
  if (clusters.length === 0) {
    return document.createElement('div');
  }

  const element = new HistoryClustersModuleElement();
  element.cluster = clusters[0];

  return element as HTMLElement;
}

export const historyClustersV2Descriptor: ModuleDescriptorV2 =
    new ModuleDescriptorV2(
        /*id=*/ 'history_clusters', ModuleHeight.DYNAMIC, createElement);
