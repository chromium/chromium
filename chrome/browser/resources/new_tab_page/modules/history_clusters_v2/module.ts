// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLazyRenderElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Cluster, URLVisit} from '../../history_cluster_types.mojom-webui.js';
import {I18nMixin, loadTimeData} from '../../i18n_setup.js';
import {HistoryClustersProxyImpl} from '../history_clusters/history_clusters_proxy.js';
import {InfoDialogElement} from '../info_dialog';
import {ModuleDescriptorV2, ModuleHeight} from '../module_descriptor.js';

import {getTemplate} from './module.html.js';

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
        [this.searchResultsPage_, ...this.cluster.visits]);
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

  private onOpenAllInTabGroupClick_() {
    const urls = [this.searchResultsPage_, ...this.cluster.visits].map(
        visit => visit.normalizedUrl);
    HistoryClustersProxyImpl.getInstance().handler.openUrlsInTabGroup(urls);
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
