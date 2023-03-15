// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';
import 'chrome://resources/cr_components/history_clusters/history_clusters_shared_style.css.js';
import './page_favicon.js';

import {ImageServiceBrowserProxy} from 'chrome://resources/cr_components/image_service/browser_proxy.js';
import {ClientId as ImageServiceClientId} from 'chrome://resources/cr_components/image_service/image_service.mojom-webui.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {URLVisit} from '../../history_cluster_types.mojom-webui.js';
import {I18nMixin, loadTimeData} from '../../i18n_setup.js';

import {getTemplate} from './tile.html.js';

export class TileModuleElement extends I18nMixin
(PolymerElement) {
  static get is() {
    return 'ntp-history-clusters-tile';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /* The visit to display. */
      visit: {
        type: Object,
        observer: 'onVisitUpdated_',
      },

      /* The label to display. */
      label_: {
        type: String,
        computed: `computeLabel_(visit.urlForDisplay)`,
      },

      /* The image url for the tile. */
      imageUrl_: {
        type: Object,
        value: null,
      },

      smallFormat: {
        type: Boolean,
        value: false,
      },
    };
  }

  visit: URLVisit;
  smallFormat: boolean;
  private imageUrl_: Url|null;

  private computeLabel_(): string {
    let domain = (new URL(this.visit.normalizedUrl.url)).hostname;
    domain = domain.replace('www.', '');
    return domain;
  }

  // Set imageUrl when visit is set/updated.
  private async onVisitUpdated_(): Promise<void> {
    const visitUrl = this.visit.normalizedUrl;
    if (visitUrl && this.visit.hasUrlKeyedImage && !this.smallFormat &&
        loadTimeData.getBoolean('isHistoryClustersImagesEnabled') &&
        this.visit.isKnownToSync) {
      const result =
          await ImageServiceBrowserProxy.getInstance().handler.getPageImageUrl(
              ImageServiceClientId.NtpQuests, visitUrl,
              {suggestImages: true, optimizationGuideImages: true});
      if (result && result.result) {
        this.imageUrl_ = result.result.imageUrl;
        return;
      }
    }
    this.imageUrl_ = null;
  }

  private hasImageUrl_(): boolean {
    return !!this.imageUrl_;
  }
}

customElements.define(TileModuleElement.is, TileModuleElement);
