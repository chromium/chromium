// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './combobox/customize_chrome_combobox.js';
import 'chrome://customize-chrome-side-panel.top-chrome/shared/sp_heading.js';
import 'chrome://customize-chrome-side-panel.top-chrome/shared/sp_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_grid/cr_grid.js';
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';

import {SpHeading} from 'chrome://customize-chrome-side-panel.top-chrome/shared/sp_heading.js';
import {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {DomRepeatEvent, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CustomizeChromePageHandlerInterface, Descriptors, WallpaperSearchResult} from './customize_chrome.mojom-webui.js';
import {CustomizeChromeApiProxy} from './customize_chrome_api_proxy.js';
import {getTemplate} from './wallpaper_search.html.js';

export interface WallpaperSearchElement {
  $: {
    descriptorMenuA: CrActionMenuElement,
    descriptorMenuB: CrActionMenuElement,
    descriptorMenuC: CrActionMenuElement,
    heading: SpHeading,
    queryInput: CrInputElement,
    submitButton: CrButtonElement,
  };
}

export class WallpaperSearchElement extends PolymerElement {
  static get is() {
    return 'customize-chrome-wallpaper-search';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      descriptors_: Object,
      emptyContainers_: Object,
      query_: String,
      results_: Object,
    };
  }

  private descriptors_: Descriptors|null;
  private emptyContainers_: number[];
  private query_: string;
  private results_: WallpaperSearchResult[];

  private pageHandler_: CustomizeChromePageHandlerInterface;

  constructor() {
    super();
    this.pageHandler_ = CustomizeChromeApiProxy.getInstance().handler;
    this.pageHandler_.getDescriptors().then(({descriptors}) => {
      if (descriptors) {
        this.descriptors_ = descriptors;
      }
    });
  }

  focusOnBackButton() {
    this.$.heading.getBackButton().focus();
  }

  private async onBackClick_() {
    this.dispatchEvent(new Event('back-click'));
  }

  private onDescriptorMenuClickA_(e: Event) {
    this.$.descriptorMenuA.showAt(e.target as HTMLElement);
  }

  private onDescriptorMenuClickB_(e: Event) {
    this.$.descriptorMenuB.showAt(e.target as HTMLElement);
  }

  private onDescriptorMenuClickC_(e: Event) {
    this.$.descriptorMenuC.showAt(e.target as HTMLElement);
  }

  private async onSearchClick_() {
    const {results} =
        await this.pageHandler_.getWallpaperSearchResults(this.query_);
    this.results_ = results;
    this.emptyContainers_ = Array.from(
        {length: results.length > 0 ? 6 - results.length : 0}, () => 0);
    this.$.queryInput.invalid = !results.length;
    this.$.queryInput.errorMessage = 'Error';
  }

  private async onResultClick_(e: DomRepeatEvent<WallpaperSearchResult>) {
    this.pageHandler_.setBackgroundToWallpaperSearchResult(e.model.item.id);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'customize-chrome-wallpaper-search': WallpaperSearchElement;
  }
}

customElements.define(WallpaperSearchElement.is, WallpaperSearchElement);
