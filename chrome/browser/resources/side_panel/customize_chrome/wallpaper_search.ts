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
import 'chrome://resources/cr_elements/cr_loading_gradient/cr_loading_gradient.js';

import {SpHeading} from 'chrome://customize-chrome-side-panel.top-chrome/shared/sp_heading.js';
import {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {assert} from 'chrome://resources/js/assert.js';
import {DomRepeatEvent, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CustomizeChromeCombobox} from './combobox/customize_chrome_combobox.js';
import {CustomizeChromePageHandlerInterface, DescriptorA, Descriptors, WallpaperSearchResult} from './customize_chrome.mojom-webui.js';
import {CustomizeChromeApiProxy} from './customize_chrome_api_proxy.js';
import {getTemplate} from './wallpaper_search.html.js';

export const DESCRIPTOR_C_VALUE =
    ['#EF4837', '#0984E3', '#F9CC18', '#23CC6A', '#474747'];

export interface WallpaperSearchElement {
  $: {
    descriptorComboboxA: CustomizeChromeCombobox,
    descriptorComboboxB: CustomizeChromeCombobox,
    descriptorMenuC: CrActionMenuElement,
    descriptorMenuD: CrActionMenuElement,
    heading: SpHeading,
    loading: HTMLElement,
    submitButton: CrButtonElement,
  };
}

function getRandomDescriptorA(descriptorArrayA: DescriptorA[]): string {
  const randomLabels =
      descriptorArrayA[Math.floor(Math.random() * descriptorArrayA.length)]
          .labels;
  return randomLabels[Math.floor(Math.random() * randomLabels.length)];
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
      descriptors_: {
        type: Object,
        value: null,
      },
      descriptorD_: {
        type: Array,
        value: DESCRIPTOR_C_VALUE,
      },
      emptyContainers_: Object,
      loading_: {
        type: Boolean,
        value: false,
      },
      results_: Object,
      submitBtnText_: {
        type: String,
        computed: 'computeSubmitBtnText_(results_)',
        value: 'Search',
      },
    };
  }

  private descriptors_: Descriptors|null;
  private descriptorD_: string[];
  private emptyContainers_: number[];
  private loading_: boolean;
  private results_: WallpaperSearchResult[];
  private selectedDescriptorA_: string|null;
  private selectedDescriptorB_: string|null;
  private selectedDescriptorC_: string|null;
  private selectedDescriptorD_: string|null;
  private submitBtnText_: string;

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

  private computeSubmitBtnText_() {
    return this.results_ && this.results_.length > 0 ? 'Search Again' :
                                                       'Search';
  }

  private async onBackClick_() {
    this.dispatchEvent(new Event('back-click'));
  }

  private onDescriptorLabelClickC_(e: DomRepeatEvent<string>) {
    this.selectedDescriptorC_ = e.model.item;
    this.$.descriptorMenuC.close();
  }

  private onDescriptorLabelClickD_(e: DomRepeatEvent<string>) {
    this.selectedDescriptorD_ = e.model.item;
    this.$.descriptorMenuC.close();
  }

  private onDescriptorMenuClickC_(e: Event) {
    this.$.descriptorMenuC.showAt(e.target as HTMLElement);
  }

  private async onSearchClick_() {
    assert(this.descriptors_);
    const descriptorA = this.selectedDescriptorA_ ||
        getRandomDescriptorA(this.descriptors_.descriptorA);
    this.loading_ = true;
    const {results} = await this.pageHandler_.getWallpaperSearchResults(
        descriptorA, this.selectedDescriptorB_, this.selectedDescriptorC_,
        this.selectedDescriptorD_);
    this.loading_ = false;
    this.results_ = results;
    this.emptyContainers_ = Array.from(
        {length: results.length > 0 ? 6 - results.length : 0}, () => 0);
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
