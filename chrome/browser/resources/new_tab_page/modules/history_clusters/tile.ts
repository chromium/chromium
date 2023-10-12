// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';
import 'chrome://resources/cr_components/history_clusters/history_clusters_shared_style.css.js';
import './page_favicon.js';

import {PageImageServiceBrowserProxy} from 'chrome://resources/cr_components/page_image_service/browser_proxy.js';
import {ClientId as PageImageServiceClientId} from 'chrome://resources/cr_components/page_image_service/page_image_service.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Annotation, URLVisit} from '../../history_cluster_types.mojom-webui.js';
import {I18nMixin} from '../../i18n_setup.js';

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

      /* Annotations to show for the visit (e.g., whether page was bookmarked).
       */
      annotation_: {
        type: String,
        computed: 'computeAnnotation_(visit)',
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
        reflectToAttribute: true,
      },

      // The texts for the discount chip.
      discount: {
        type: String,
      },

      hasDiscount: {
        type: Boolean,
        computed: `computeHasDiscount_(discount)`,
        reflectToAttribute: true,
      },

      /* The label of the tile in a11y mode. */
      tileLabel_: {
        type: String,
        computed: `computeTileLabel_(discount, label_)`,
      },
    };
  }

  visit: URLVisit;
  smallFormat: boolean;
  discount: string;
  hasDiscount: boolean;
  private imageUrl_: Url|null;
  private label_: string;

  hasImageUrl(): boolean {
    return !!this.imageUrl_;
  }

  private computeAnnotation_(_visit: URLVisit): string {
    if (Annotation.kBookmarked in this.visit.annotations) {
      return loadTimeData.getString('modulesJourneysBookmarked');
    }
    return '';
  }

  private computeLabel_(): string {
    let domain = (new URL(this.visit.normalizedUrl.url)).hostname;
    domain = domain.replace('www.', '');
    return domain;
  }

  private computeHasDiscount_(): boolean {
    return !!this.discount && this.discount.length !== 0;
  }

  // Set imageUrl when visit is set/updated.
  private async onVisitUpdated_(): Promise<void> {
    const visitUrl = this.visit.normalizedUrl;
    if (visitUrl && this.visit.hasUrlKeyedImage && !this.smallFormat &&
        this.visit.isKnownToSync) {
      const result =
          await PageImageServiceBrowserProxy.getInstance()
              .handler.getPageImageUrl(
                  PageImageServiceClientId.NtpQuests, visitUrl,
                  {suggestImages: false, optimizationGuideImages: true});
      const success = !!(result && result.result);
      chrome.metricsPrivate.recordBoolean(
          'NewTabPage.HistoryClusters.ImageLoadSuccess', success);
      if (success) {
        this.imageUrl_ = result!.result!.imageUrl;
        return;
      }
    }
    this.imageUrl_ = null;
  }

  private computeTileLabel_(): string {
    const labelTexts =
        [this.visit.pageTitle, this.label_, this.visit.relativeDate];
    if (!!this.discount && this.discount.length !== 0) {
      labelTexts.push(this.discount);
    }
    return labelTexts.join(', ');
  }
}

customElements.define(TileModuleElement.is, TileModuleElement);
