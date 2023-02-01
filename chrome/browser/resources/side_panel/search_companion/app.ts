// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../strings.m.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';
import {SearchCompanionApiProxy, SearchCompanionApiProxyImpl} from './search_companion_api_proxy.js';

export interface SearchCompanionAppElement {
  $: {};
}

export class SearchCompanionAppElement extends PolymerElement {
  static get is() {
    return 'search-companion-app';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {};
  }

  private searchCompanionApi_: SearchCompanionApiProxy =
      SearchCompanionApiProxyImpl.getInstance();
  private currentUrl_: string = 'No new page updates';
  private currentSuggestResponse_: string;
  private currentContentAnnotations_: string;
  private currentViewportImages_: string;

  override connectedCallback() {
    super.connectedCallback();

    // Setup data listeners
    this.searchCompanionApi_.callbackRouter.onURLChanged.addListener(
        (newUrl: string) => {
          this.currentUrl_ = newUrl;
        });
    this.searchCompanionApi_.callbackRouter.onNewZeroSuggestPrefixData
        .addListener((suggestResponse: string) => {
          this.currentSuggestResponse_ = suggestResponse;
        });
    this.searchCompanionApi_.callbackRouter
        .onNewOptimizationGuidePageAnnotations.addListener(
            (contentAnnotations: string) => {
              this.currentContentAnnotations_ = contentAnnotations;
            });
    this.searchCompanionApi_.callbackRouter.onNewViewportImages.addListener(
        (imagesString: string) => {
          this.currentViewportImages_ = imagesString;
        });

    this.searchCompanionApi_.showUi();
  }
}
declare global {
  interface HTMLElementTagNameMap {
    'search-companion-app': SearchCompanionAppElement;
  }
}
customElements.define(SearchCompanionAppElement.is, SearchCompanionAppElement);