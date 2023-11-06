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
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {hexColorToSkColor} from 'chrome://resources/js/color_utils.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {Token} from 'chrome://resources/mojo/mojo/public/mojom/base/token.mojom-webui.js';
import {Debouncer, DomRepeatEvent, PolymerElement, timeOut} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CustomizeChromePageCallbackRouter, CustomizeChromePageHandlerInterface, Theme} from '../customize_chrome.mojom-webui.js';
import {CustomizeChromeApiProxy} from '../customize_chrome_api_proxy.js';
import {DescriptorA, DescriptorDValue, Descriptors, WallpaperSearchHandlerInterface, WallpaperSearchResult, WallpaperSearchStatus} from '../wallpaper_search.mojom-webui.js';

import {CustomizeChromeCombobox} from './combobox/customize_chrome_combobox.js';
import {getTemplate} from './wallpaper_search.html.js';
import {WallpaperSearchProxy} from './wallpaper_search_proxy.js';

export const DESCRIPTOR_D_VALUE =
    ['#ef4837', '#0984e3', '#f9cc18', '#23cc6a', '#474747'];

export interface ErrorState {
  title: string;
  description: string;
}

export interface WallpaperSearchElement {
  $: {
    customColorContainer: HTMLElement,
    descriptorComboboxA: CustomizeChromeCombobox,
    descriptorComboboxB: CustomizeChromeCombobox,
    descriptorComboboxC: CustomizeChromeCombobox,
    descriptorMenuD: CrActionMenuElement,
    heading: SpHeading,
    hueSlider: ThemeHueSliderDialogElement,
    loading: HTMLElement,
    submitButton: CrButtonElement,
    wallpaperSearch: HTMLElement,
  };
}

function getRandomDescriptorA(descriptorArrayA: DescriptorA[]): string {
  const randomLabels =
      descriptorArrayA[Math.floor(Math.random() * descriptorArrayA.length)]
          .labels;
  return randomLabels[Math.floor(Math.random() * randomLabels.length)];
}

const WallpaperSearchElementBase = I18nMixin(PolymerElement);

export class WallpaperSearchElement extends WallpaperSearchElementBase {
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
      errorState_: {
        type: Object,
        computed: 'computeErrorState_(status_)',
      },
      emptyContainers_: Object,
      loading_: {
        type: Boolean,
        value: false,
      },
      results_: Object,
      selectedHue_: Number,
      status_: {
        type: WallpaperSearchStatus,
        value: WallpaperSearchStatus.kOk,
      },
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
  private errorCallback_: (() => Promise<void>)|undefined;
  private errorState_: ErrorState|null = null;
  private loading_: boolean;
  private results_: WallpaperSearchResult[];
  private selectedDefaultColor_: string|undefined;
  private selectedDescriptorA_: string|null;
  private selectedDescriptorB_: string|null;
  private selectedDescriptorC_: string|null;
  private selectedDescriptorD_: DescriptorDValue|null;
  private selectedHue_: number|undefined;
  private status_: WallpaperSearchStatus;
  private submitBtnText_: string;
  private theme_: Theme|undefined;

  private callbackRouter_: CustomizeChromePageCallbackRouter;
  private pageHandler_: CustomizeChromePageHandlerInterface;
  private wallpaperSearchHandler_: WallpaperSearchHandlerInterface;
  private setThemeListenerId_: number|null = null;
  private loadingUiResizeObserver_: ResizeObserver|null = null;
  private loadingUiDebouncer_: Debouncer|null = null;

  constructor() {
    super();
    this.callbackRouter_ = CustomizeChromeApiProxy.getInstance().callbackRouter;
    this.pageHandler_ = CustomizeChromeApiProxy.getInstance().handler;
    this.wallpaperSearchHandler_ = WallpaperSearchProxy.getHandler();
    this.fetchDescriptors_();
  }

  override connectedCallback() {
    super.connectedCallback();
    this.setThemeListenerId_ =
        this.callbackRouter_.setTheme.addListener((theme: Theme) => {
          this.theme_ = theme;
        });
    this.pageHandler_.updateTheme();
    this.loadingUiResizeObserver_ = new ResizeObserver(() => {
      // Timeout of 20ms was decided by manual testing to see how often the
      // resizes can be debounced before appearing janky.
      this.loadingUiDebouncer_ = Debouncer.debounce(
          this.loadingUiDebouncer_, timeOut.after(20),
          () => this.generateLoadingUi_());
    });
    this.loadingUiResizeObserver_.observe(this);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    assert(this.setThemeListenerId_);
    this.callbackRouter_.removeListener(this.setThemeListenerId_);
    this.loadingUiResizeObserver_!.disconnect();
    this.loadingUiResizeObserver_ = null;
  }

  focusOnBackButton() {
    this.$.heading.getBackButton().focus();
  }

  private computeErrorState_() {
    switch (this.status_) {
      case WallpaperSearchStatus.kOk:
        return null;
      case WallpaperSearchStatus.kError:
        return {
          title: this.i18n('genericErrorTitle'),
          description: this.i18n('genericErrorDescription'),
          callToAction: this.i18n('tryAgain'),
        };
    }
  }

  private computeSubmitBtnText_() {
    return this.results_ && this.results_.length > 0 ?
        loadTimeData.getString('wallpaperSearchSubmitAgainBtn') :
        loadTimeData.getString('wallpaperSearchSubmitBtn');
  }

  private async fetchDescriptors_() {
    this.wallpaperSearchHandler_.getDescriptors().then(({descriptors}) => {
      if (descriptors) {
        this.descriptors_ = descriptors;
        this.errorCallback_ = undefined;
      } else {
        this.errorCallback_ = () => this.fetchDescriptors_();
        this.status_ = WallpaperSearchStatus.kError;
      }
    });
  }

  /**
   * The loading gradient is rendered using a SVG clip path. As typical CSS
   * layouts such as grid cannot apply to clip paths, this ResizeObserver
   * callback resizes the loading tiles based on the current width of the
   * side panel.
   */
  private generateLoadingUi_() {
    const availableWidth = this.$.wallpaperSearch.offsetWidth;
    if (availableWidth === 0) {
      // Wallpaper search is likely hidden.
      return;
    }

    const columns = 3;
    const gapBetweenTiles = 10;
    const tileSize =
        (availableWidth - (gapBetweenTiles * (columns - 1))) / columns;

    const svg = this.$.loading.querySelector('svg')!;
    const rects = svg.querySelectorAll<SVGRectElement>('rect');
    const rows = Math.ceil(rects.length / columns);

    svg.setAttribute('width', `${availableWidth}`);
    svg.setAttribute(
        'height', `${(rows * tileSize) + ((rows - 1) * gapBetweenTiles)}`);

    for (let row = 0; row < rows; row++) {
      for (let column = 0; column < columns; column++) {
        const rect = rects[column + (row * columns)];
        if (!rect) {
          return;
        }
        rect.setAttribute('height', `${tileSize}`);
        rect.setAttribute('width', `${tileSize}`);
        rect.setAttribute('x', `${column * (tileSize + gapBetweenTiles)}`);
        rect.setAttribute('y', `${row * (tileSize + gapBetweenTiles)}`);
      }
    }
  }

  private isBackgroundSelected_(id: Token): boolean {
    return !!(
        this.theme_ && this.theme_.backgroundImage &&
        this.theme_.backgroundImage.localBackgroundId &&
        this.theme_.backgroundImage.localBackgroundId.low === id.low &&
        this.theme_.backgroundImage.localBackgroundId.high === id.high);
  }

  private isDefaultColorSelected_(color: string): boolean {
    return color === this.selectedDefaultColor_;
  }

  private async onBackClick_() {
    this.dispatchEvent(new Event('back-click'));
  }

  private onCustomColorClick_() {
    this.$.hueSlider.showAt(this.$.customColorContainer);
  }

  private onErrorClick_() {
    this.status_ = WallpaperSearchStatus.kOk;
    if (this.errorCallback_) {
      this.errorCallback_();
    }
  }

  private onDefaultColorClick_(e: DomRepeatEvent<string>) {
    this.selectedHue_ = undefined;
    this.selectedDefaultColor_ = e.model.item;
    this.selectedDescriptorD_ = {
      color: hexColorToSkColor(this.selectedDefaultColor_),
    };
  }

  private async onSelectedHueChanged_() {
    this.selectedDefaultColor_ = undefined;
    this.selectedHue_ = this.$.hueSlider.selectedHue;
    this.selectedDescriptorD_ = {hue: this.selectedHue_};
  }

  private async onSearchClick_() {
    assert(this.descriptors_);
    this.selectedDescriptorA_ = this.selectedDescriptorA_ ||
        getRandomDescriptorA(this.descriptors_.descriptorA);
    this.loading_ = true;
    this.results_ = [];
    this.emptyContainers_ = [];
    const {status, results} =
        await this.wallpaperSearchHandler_.getWallpaperSearchResults(
            this.selectedDescriptorA_, this.selectedDescriptorB_,
            this.selectedDescriptorC_, this.selectedDescriptorD_);
    this.loading_ = false;
    this.results_ = results;
    this.status_ = status;
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
