// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../module_header.js';
import './tile.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Cluster} from '../../history_cluster_types.mojom-webui.js';
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
  const data =
      await HistoryClustersProxyImpl.getInstance().handler.getCluster();
  if (!data.cluster) {
    return null;
  }

  const element = new HistoryClustersModuleElement();
  element.cluster = data.cluster!;
  return element;
}

export const historyClustersDescriptor: ModuleDescriptor = new ModuleDescriptor(
    /*id=*/ 'history-clusters', createElement);
