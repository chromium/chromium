// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../module_header.js';

import {Cluster, QueryResult} from 'chrome://resources/cr_components/history_clusters/history_clusters.mojom-webui.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {I18nMixin} from '../../i18n_setup.js';
import {ModuleDescriptor} from '../module_descriptor.js';

import {HistoryClustersProxyImpl} from './history_clusters_proxy.js';
import {getTemplate} from './module.html.js';

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
      /** The cluster displayed by this element. */
      cluster: Object,
    };
  }

  cluster: Cluster;
}

customElements.define(
    HistoryClustersModuleElement.is, HistoryClustersModuleElement);

async function createElement(): Promise<HTMLElement|null> {
  const fetchClusters: Promise<Cluster[]> = new Promise(resolve => {
    const callbackRouter =
        HistoryClustersProxyImpl.getInstance().callbackRouter;
    const listenerId = callbackRouter.onClustersQueryResult.addListener(
        (result: QueryResult) => {
          callbackRouter.removeListener(listenerId);
          resolve(result.clusters);
        });

    // TODO(crbug.com/1409691): Replace this call with a more suitable one
    // that does not require superfluous parameters.
    HistoryClustersProxyImpl.getInstance().handler.startQueryClusters(
        '', false);
  });
  const clusters = await fetchClusters;
  if (clusters.length === 0) {
    return null;
  }

  const element = new HistoryClustersModuleElement();
  element.cluster = clusters[0];
  return element;
}

export const historyClustersDescriptor: ModuleDescriptor = new ModuleDescriptor(
    /*id=*/ 'history-clusters', createElement);
