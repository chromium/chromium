// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../check_mark_wrapper.js';
import './combobox/customize_chrome_combobox.js';
import 'chrome://customize-chrome-side-panel.top-chrome/shared/sp_heading.js';
import 'chrome://customize-chrome-side-panel.top-chrome/shared/sp_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_grid/cr_grid.js';
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/cr_loading_gradient/cr_loading_gradient.js';
import 'chrome://resources/cr_components/theme_color_picker/theme_hue_slider_dialog.js';

import {SpHeading} from 'chrome://customize-chrome-side-panel.top-chrome/shared/sp_heading.js';
import {ThemeHueSliderDialogElement} from 'chrome://resources/cr_components/theme_color_picker/theme_hue_slider_dialog.js';
import {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {assert} from 'chrome://resources/js/assert.js';
import {hexColorToSkColor} from 'chrome://resources/js/color_utils.js';
import {Token} from 'chrome://resources/mojo/mojo/public/mojom/base/token.mojom-webui.js';
import {DomRepeatEvent, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CustomizeChromeCombobox} from './combobox/customize_chrome_combobox.js';
import {CustomizeChromePageCallbackRouter, CustomizeChromePageHandlerInterface, Theme} from '../customize_chrome.mojom-webui.js';
import {CustomizeChromeApiProxy} from '../customize_chrome_api_proxy.js';
import {getTemplate} from './wallpaper_search.html.js';
import {DescriptorA, DescriptorDValue, Descriptors, WallpaperSearchHandlerInterface, WallpaperSearchResult} from '../wallpaper_search.mojom-webui.js';
import {WallpaperSearchProxy} from './wallpaper_search_proxy.js';

export const DESCRIPTOR_D_VALUE =
    ['#ef4837', '#0984e3', '#f9cc18', '#23cc6a', '#474747'];

export interface WallpaperSearchElement {
  $: {
    descriptorComboboxA: CustomizeChromeCombobox,
    descriptorComboboxB: CustomizeChromeCombobox,
    descriptorComboboxC: CustomizeChromeCombobox,
    descriptorMenuD: CrActionMenuElement,
    heading: SpHeading,
    hueSlider: ThemeHueSliderDialogElement,
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
        value: DESCRIPTOR_D_VALUE,
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
      theme_: {
        type: Object,
        value: undefined,
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
  private selectedDescriptorD_: DescriptorDValue|null;
  private submitBtnText_: string;
  private theme_: Theme|undefined;

  private callbackRouter_: CustomizeChromePageCallbackRouter;
  private pageHandler_: CustomizeChromePageHandlerInterface;
  private wallpaperSearchHandler_: WallpaperSearchHandlerInterface;
  private setThemeListenerId_: number|null = null;

  constructor() {
    super();
    this.callbackRouter_ = CustomizeChromeApiProxy.getInstance().callbackRouter;
    this.pageHandler_ = CustomizeChromeApiProxy.getInstance().handler;
    this.wallpaperSearchHandler_ = WallpaperSearchProxy.getHandler();
    this.wallpaperSearchHandler_.getDescriptors().then(({descriptors}) => {
      if (descriptors) {
        this.descriptors_ = descriptors;
      }
    });
  }

  override connectedCallback() {
    super.connectedCallback();
    this.setThemeListenerId_ =
        this.callbackRouter_.setTheme.addListener((theme: Theme) => {
          this.theme_ = theme;
        });
    this.pageHandler_.updateTheme();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    assert(this.setThemeListenerId_);
    this.callbackRouter_.removeListener(this.setThemeListenerId_);
  }

  focusOnBackButton() {
    this.$.heading.getBackButton().focus();
  }

  private computeSubmitBtnText_() {
    return this.results_ && this.results_.length > 0 ? 'Search Again' :
                                                       'Search';
  }

  private isBackgroundSelected_(id: Token): boolean {
    return !!(
        this.theme_ && this.theme_.backgroundImage &&
        this.theme_.backgroundImage.localBackgroundId &&
        this.theme_.backgroundImage.localBackgroundId.low === id.low &&
        this.theme_.backgroundImage.localBackgroundId.high === id.high);
  }

  private async onBackClick_() {
    this.dispatchEvent(new Event('back-click'));
  }

  private onCustomColorClick_(e: Event) {
    this.$.hueSlider.showAt(e.target as HTMLElement);
  }

  private onSelectedColorChanged_(e: DomRepeatEvent<string>) {
    this.selectedDescriptorD_ = {color: hexColorToSkColor(e.model.item)};
  }

  private async onSelectedHueChanged_() {
    this.selectedDescriptorD_ = {hue: this.$.hueSlider.selectedHue};
  }

  private async onSearchClick_() {
    assert(this.descriptors_);
    this.selectedDescriptorA_ = this.selectedDescriptorA_ ||
        getRandomDescriptorA(this.descriptors_.descriptorA);
    this.loading_ = true;
    this.results_ = [];
    this.emptyContainers_ = [];
    const {results} =
        await this.wallpaperSearchHandler_.getWallpaperSearchResults(
            this.selectedDescriptorA_, this.selectedDescriptorB_,
            this.selectedDescriptorC_, this.selectedDescriptorD_);
    this.loading_ = false;
    this.results_ = results;
    this.emptyContainers_ = Array.from(
        {length: results.length > 0 ? 6 - results.length : 0}, () => 0);
  }

  private async onResultClick_(e: DomRepeatEvent<WallpaperSearchResult>) {
    this.wallpaperSearchHandler_.setBackgroundToWallpaperSearchResult(
        e.model.item.id);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'customize-chrome-wallpaper-search': WallpaperSearchElement;
  }
}

customElements.define(WallpaperSearchElement.is, WallpaperSearchElement);
