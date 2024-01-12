// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../check_mark_wrapper.js';
import './combobox/customize_chrome_combobox.js';
import 'chrome://customize-chrome-side-panel.top-chrome/shared/sp_heading.js';
import 'chrome://customize-chrome-side-panel.top-chrome/shared/sp_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_feedback_buttons/cr_feedback_buttons.js';
import 'chrome://resources/cr_elements/cr_grid/cr_grid.js';
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/cr_loading_gradient/cr_loading_gradient.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_components/theme_color_picker/theme_hue_slider_dialog.js';
import 'chrome://resources/polymer/v3_0/paper-ripple/paper-ripple.js';

import {SpHeading} from 'chrome://customize-chrome-side-panel.top-chrome/shared/sp_heading.js';
import {ThemeHueSliderDialogElement} from 'chrome://resources/cr_components/theme_color_picker/theme_hue_slider_dialog.js';
import {CrA11yAnnouncerElement, getInstance as getAnnouncerInstance} from 'chrome://resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {CrFeedbackButtonsElement, CrFeedbackOption} from 'chrome://resources/cr_elements/cr_feedback_buttons/cr_feedback_buttons.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {hexColorToSkColor} from 'chrome://resources/js/color_utils.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {Token} from 'chrome://resources/mojo/mojo/public/mojom/base/token.mojom-webui.js';
import {Debouncer, DomRepeatEvent, PolymerElement, timeOut} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CustomizeChromeAction, recordCustomizeChromeAction} from '../common.js';
import {CustomizeChromePageCallbackRouter, CustomizeChromePageHandlerInterface, Theme} from '../customize_chrome.mojom-webui.js';
import {CustomizeChromeApiProxy} from '../customize_chrome_api_proxy.js';
import {DescriptorA, DescriptorB, DescriptorDValue, Descriptors, Inspirations, ResultDescriptors, UserFeedback, WallpaperSearchClientCallbackRouter, WallpaperSearchHandlerInterface, WallpaperSearchResult, WallpaperSearchStatus} from '../wallpaper_search.mojom-webui.js';
import {WindowProxy} from '../window_proxy.js';

import {ComboboxGroup, ComboboxItem, CustomizeChromeCombobox} from './combobox/customize_chrome_combobox.js';
import {getTemplate} from './wallpaper_search.html.js';
import {WallpaperSearchProxy} from './wallpaper_search_proxy.js';

export const DESCRIPTOR_D_VALUE: ColorDescriptor[] = [
  {
    hex: '#ef4837',
    name: 'colorRed',
  },
  {
    hex: '#0984e3',
    name: 'colorBlue',
  },
  {
    hex: '#f9cc18',
    name: 'colorYellow',
  },
  {
    hex: '#23cc6a',
    name: 'colorGreen',
  },
  {
    hex: '#474747',
    name: 'colorBlack',
  },
];

interface ColorDescriptor {
  hex: string;
  name: string;
}

interface ComboxItems {
  a: ComboboxGroup[];
  b: ComboboxItem[];
  c: ComboboxItem[];
}

export interface ErrorState {
  title: string;
  description: string;
  callToAction: string;
}

export interface WallpaperSearchElement {
  $: {
    customColorContainer: HTMLElement,
    deleteSelectedHueButton: HTMLElement,
    descriptorComboboxA: CustomizeChromeCombobox,
    descriptorComboboxB: CustomizeChromeCombobox,
    descriptorComboboxC: CustomizeChromeCombobox,
    descriptorMenuD: CrActionMenuElement,
    error: HTMLElement,
    feedbackButtons: CrFeedbackButtonsElement,
    heading: SpHeading,
    historyCard: HTMLElement,
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

function recordStatusChange(status: WallpaperSearchStatus) {
  chrome.metricsPrivate.recordEnumerationValue(
      'NewTabPage.WallpaperSearch.Status', status,
      WallpaperSearchStatus.MAX_VALUE);
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
      comboboxItems_: Array,
      descriptors_: {
        type: Object,
        value: null,
      },
      descriptorD_: {
        type: Array,
        value: DESCRIPTOR_D_VALUE.map((value) => value.hex),
      },
      errorState_: {
        type: Object,
        computed: 'computeErrorState_(status_, history_)',
      },
      emptyHistoryContainers_: Object,
      emptyResultContainers_: Object,
      expandedCategories_: Object,
      loading_: {
        type: Boolean,
        value: false,
      },
      history_: Object,
      inspirationCardEnabled_: {
        type: Boolean,
        value: () =>
            loadTimeData.getBoolean('wallpaperSearchInspirationCardEnabled'),
      },
      inspirations_: Object,
      resultsDescriptors_: Object,
      results_: Object,
      selectedFeedbackOption_: {
        type: Number,
        value: CrFeedbackOption.UNSPECIFIED,
      },
      selectedDescriptorA_: {
        type: String,
        observer: 'onSubjectDescriptorChange_',
      },
      selectedDescriptorB_: {
        type: String,
        observer: 'onStyleDescriptorChange_',
      },
      selectedDescriptorC_: {
        type: String,
        observer: 'onMoodDescriptorChange_',
      },
      selectedDescriptorD_: {
        type: Object,
        observer: 'onColorDescriptorChange_',
      },
      selectedHue_: {
        type: Number,
        value: null,
      },
      status_: {
        type: WallpaperSearchStatus,
        value: WallpaperSearchStatus.kOk,
        observer: 'onStatusChange_',
      },
      theme_: {
        type: Object,
        value: undefined,
      },
    };
  }

  private comboboxItems_: ComboxItems|null;
  private descriptors_: Descriptors|null;
  private descriptorD_: string[];
  private emptyHistoryContainers_: number[] = [];
  private emptyResultContainers_: number[] = [];
  private errorCallback_: (() => void)|undefined;
  private errorState_: ErrorState|null = null;
  private expandedCategories_: {[categoryIndex: number]: boolean} = {};
  private history_: WallpaperSearchResult[] = [];
  private inspirations_: Inspirations|null;
  private inspirationCardEnabled_: boolean;
  private loading_: boolean;
  private results_: WallpaperSearchResult[] = [];
  private resultsDescriptors_: ResultDescriptors|null = null;
  private resultsPromises_: Array<Promise<
      {status: WallpaperSearchStatus, results: WallpaperSearchResult[]}>> = [];
  private selectedDefaultColor_: string|undefined;
  private selectedDescriptorA_: string|null;
  private selectedDescriptorB_: string|null;
  private selectedDescriptorC_: string|null;
  private selectedDescriptorD_: DescriptorDValue|null;
  private selectedFeedbackOption_: CrFeedbackOption;
  private selectedHue_: number|null;
  private status_: WallpaperSearchStatus;
  private theme_: Theme|undefined;

  private callbackRouter_: CustomizeChromePageCallbackRouter;
  private pageHandler_: CustomizeChromePageHandlerInterface;
  private wallpaperSearchCallbackRouter_: WallpaperSearchClientCallbackRouter;
  private wallpaperSearchHandler_: WallpaperSearchHandlerInterface;
  private setThemeListenerId_: number|null = null;
  private setHistoryListenerId_: number|null = null;
  private loadingUiResizeObserver_: ResizeObserver|null = null;
  private loadingUiDebouncer_: Debouncer|null = null;

  constructor() {
    super();
    this.callbackRouter_ = CustomizeChromeApiProxy.getInstance().callbackRouter;
    this.pageHandler_ = CustomizeChromeApiProxy.getInstance().handler;
    this.wallpaperSearchHandler_ = WallpaperSearchProxy.getInstance().handler;
    this.wallpaperSearchCallbackRouter_ =
        WallpaperSearchProxy.getInstance().callbackRouter;
    this.fetchDescriptors_();
    this.wallpaperSearchHandler_.getInspirations().then(({inspirations}) => {
      this.inspirations_ = inspirations;
    });
  }

  override connectedCallback() {
    super.connectedCallback();
    this.setThemeListenerId_ =
        this.callbackRouter_.setTheme.addListener((theme: Theme) => {
          this.theme_ = theme;
        });
    this.pageHandler_.updateTheme();
    this.setHistoryListenerId_ =
        this.wallpaperSearchCallbackRouter_.setHistory.addListener(
            (history: WallpaperSearchResult[]) => {
              this.history_ = history;
              this.emptyHistoryContainers_ = this.calculateEmptyTiles(history);
            });
    this.wallpaperSearchHandler_.updateHistory();
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
    assert(this.setHistoryListenerId_);
    this.callbackRouter_.removeListener(this.setThemeListenerId_);
    this.wallpaperSearchCallbackRouter_.removeListener(
        this.setHistoryListenerId_);
    this.loadingUiResizeObserver_!.disconnect();
    this.loadingUiResizeObserver_ = null;
  }

  focusOnBackButton() {
    this.$.heading.getBackButton().focus();
  }

  private calculateEmptyTiles(filledTiles: WallpaperSearchResult[]): number[] {
    return Array.from(
        {length: filledTiles.length > 0 ? 6 - filledTiles.length : 0}, () => 0);
  }

  private computeErrorState_() {
    switch (this.status_) {
      case WallpaperSearchStatus.kOk:
        return null;
      case WallpaperSearchStatus.kError:
        return {
          title: this.i18n('genericErrorTitle'),
          description: this.shouldShowHistory_() ?
              this.i18n('genericErrorDescriptionWithHistory') :
              this.i18n('genericErrorDescription'),
          callToAction: this.i18n('tryAgain'),
        };
      case WallpaperSearchStatus.kRequestThrottled:
        return {
          title: this.i18n('requestThrottledTitle'),
          description: this.i18n('requestThrottledDescription'),
          callToAction: this.i18n('ok'),
        };
      case WallpaperSearchStatus.kOffline:
        return {
          title: this.i18n('offlineTitle'),
          description: this.shouldShowHistory_() ?
              this.i18n('offlineDescriptionWithHistory') :
              this.i18n('offlineDescription'),
          callToAction: this.i18n('ok'),
        };
    }
  }

  private expandCategoryForDescriptorA_(label: string) {
    if (!this.descriptors_) {
      return;
    }
    const categoryGroupIndex = this.descriptors_.descriptorA.findIndex(
        group => group.labels.includes(label));
    if (categoryGroupIndex >= 0) {
      this.set(`expandedCategories_.${categoryGroupIndex}`, true);
    }
  }

  private async fetchDescriptors_() {
    this.wallpaperSearchHandler_.getDescriptors().then(({descriptors}) => {
      if (descriptors) {
        this.descriptors_ = descriptors;
        this.comboboxItems_ = {
          a: descriptors.descriptorA.map((group) => {
            return {
              label: group.category,
              items: group.labels.map((label) => {
                return {label};
              }),
            };
          }),
          b: descriptors.descriptorB,
          c: descriptors.descriptorC.map((label) => {
            return {label};
          }),
        };
        this.errorCallback_ = undefined;
        recordStatusChange(WallpaperSearchStatus.kOk);
      } else {
        this.errorCallback_ = () => this.fetchDescriptors_();
        this.status_ = WindowProxy.getInstance().onLine ?
            WallpaperSearchStatus.kError :
            WallpaperSearchStatus.kOffline;
        recordStatusChange(this.status_);
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

  private getBackgroundCheckedStatus_(id: Token): string {
    return this.isBackgroundSelected_(id) ? 'true' : 'false';
  }

  private getColorCheckedStatus_(defaultColor: string): string {
    return this.isColorSelected_(defaultColor) ? 'true' : 'false';
  }

  private getColorLabel_(defaultColor: string): string {
    const descriptor =
        DESCRIPTOR_D_VALUE.find((color) => color.hex === defaultColor);
    return descriptor ? loadTimeData.getString(descriptor.name) : '';
  }

  private getCustomColorCheckedStatus_(): string {
    return this.selectedHue_ !== null ? 'true' : 'false';
  }

  private getHistoryResultAriaLabel_(
      index: number, result: WallpaperSearchResult): string {
    if (!result.descriptors || !result.descriptors.subject) {
      return loadTimeData.getStringF(
          'wallpaperSearchHistoryResultLabelNoDescriptor', index + 1);
    } else if (result.descriptors.style && result.descriptors.mood) {
      return loadTimeData.getStringF(
          'wallpaperSearchHistoryResultLabelBC', index + 1,
          result.descriptors.subject, result.descriptors.style,
          result.descriptors.mood);
    } else if (result.descriptors.style) {
      return loadTimeData.getStringF(
          'wallpaperSearchHistoryResultLabelB', index + 1,
          result.descriptors.subject, result.descriptors.style);
    } else if (result.descriptors.mood) {
      return loadTimeData.getStringF(
          'wallpaperSearchHistoryResultLabelC', index + 1,
          result.descriptors.subject, result.descriptors.mood);
    }
    return loadTimeData.getStringF(
        'wallpaperSearchHistoryResultLabel', index + 1,
        result.descriptors.subject);
  }

  private getResultAriaLabel_(index: number): string {
    assert(this.resultsDescriptors_ && this.resultsDescriptors_.subject);
    if (this.resultsDescriptors_.style && this.resultsDescriptors_.mood) {
      return loadTimeData.getStringF(
          'wallpaperSearchResultLabelBC', index + 1,
          this.resultsDescriptors_.subject, this.resultsDescriptors_.style,
          this.resultsDescriptors_.mood);
    } else if (this.resultsDescriptors_.style) {
      return loadTimeData.getStringF(
          'wallpaperSearchResultLabelB', index + 1,
          this.resultsDescriptors_.subject, this.resultsDescriptors_.style);
    } else if (this.resultsDescriptors_.mood) {
      return loadTimeData.getStringF(
          'wallpaperSearchResultLabelC', index + 1,
          this.resultsDescriptors_.subject, this.resultsDescriptors_.mood);
    }
    return loadTimeData.getStringF(
        'wallpaperSearchResultLabel', index + 1,
        this.resultsDescriptors_.subject);
  }

  private isBackgroundSelected_(id: Token): boolean {
    return !!(
        this.theme_ && this.theme_.backgroundImage &&
        this.theme_.backgroundImage.localBackgroundId &&
        this.theme_.backgroundImage.localBackgroundId.low === id.low &&
        this.theme_.backgroundImage.localBackgroundId.high === id.high);
  }

  private isColorSelected_(defaultColor: string): boolean {
    return defaultColor === this.selectedDefaultColor_;
  }

  private isOptionSelectedInDescriptorB_(option: DescriptorB): boolean {
    return option.label === this.selectedDescriptorB_;
  }

  private async onBackClick_() {
    this.dispatchEvent(new Event('back-click'));
  }

  private onComboboxCategoryClick_(e: DomRepeatEvent<DescriptorA>) {
    const index = e.model.index;
    this.set(`expandedCategories_.${index}`, !this.expandedCategories_[index]);
  }

  private onCustomColorClick_() {
    this.$.hueSlider.showAt(this.$.customColorContainer);
  }

  private onErrorClick_() {
    this.status_ = WallpaperSearchStatus.kOk;
    recordStatusChange(this.status_);
    if (this.errorCallback_) {
      this.errorCallback_();
    }
  }

  private onDefaultColorClick_(e: DomRepeatEvent<string>) {
    this.selectedHue_ = null;
    this.selectedDefaultColor_ = e.model.item;
    this.selectedDescriptorD_ = {
      color: hexColorToSkColor(this.selectedDefaultColor_),
    };
  }

  private onColorDescriptorChange_() {
    recordCustomizeChromeAction(
        CustomizeChromeAction.WALLPAPER_SEARCH_COLOR_DESCRIPTOR_UPDATED);
  }

  private onMoodDescriptorChange_() {
    recordCustomizeChromeAction(
        CustomizeChromeAction.WALLPAPER_SEARCH_MOOD_DESCRIPTOR_UPDATED);
  }

  private onStyleDescriptorChange_() {
    recordCustomizeChromeAction(
        CustomizeChromeAction.WALLPAPER_SEARCH_STYLE_DESCRIPTOR_UPDATED);
  }

  private onSubjectDescriptorChange_() {
    recordCustomizeChromeAction(
        CustomizeChromeAction.WALLPAPER_SEARCH_SUBJECT_DESCRIPTOR_UPDATED);
  }

  private onFeedbackSelectedOptionChanged_(
      e: CustomEvent<{value: CrFeedbackOption}>) {
    this.selectedFeedbackOption_ = e.detail.value;
    switch (e.detail.value) {
      case CrFeedbackOption.UNSPECIFIED:
        this.wallpaperSearchHandler_.setUserFeedback(UserFeedback.kUnspecified);
        return;
      case CrFeedbackOption.THUMBS_UP:
        recordCustomizeChromeAction(
            CustomizeChromeAction.WALLPAPER_SEARCH_THUMBS_UP_SELECTED);
        this.wallpaperSearchHandler_.setUserFeedback(UserFeedback.kThumbsUp);
        return;
      case CrFeedbackOption.THUMBS_DOWN:
        recordCustomizeChromeAction(
            CustomizeChromeAction.WALLPAPER_SEARCH_THUMBS_DOWN_SELECTED);
        this.wallpaperSearchHandler_.setUserFeedback(UserFeedback.kThumbsDown);
        return;
    }
  }

  private onHistoryImageClick_(e: DomRepeatEvent<WallpaperSearchResult>) {
    recordCustomizeChromeAction(
        CustomizeChromeAction.WALLPAPER_SEARCH_HISTORY_IMAGE_SELECTED);
    this.wallpaperSearchHandler_.setBackgroundToHistoryImage(
        e.model.item.id, e.model.item.descriptors ?? {});
  }

  private onLearnMoreClick_(e: Event) {
    e.preventDefault();
    this.wallpaperSearchHandler_.openHelpArticle();
  }

  private onSelectedHueChanged_() {
    this.selectedDefaultColor_ = undefined;
    this.selectedHue_ = this.$.hueSlider.selectedHue;
    this.selectedDescriptorD_ = {hue: this.selectedHue_};
  }

  private onSelectedHueDelete_() {
    this.selectedHue_ = null;
    this.selectedDescriptorD_ = null;
    this.$.hueSlider.hide();
    this.$.customColorContainer.focus();
  }

  private async onSearchClick_() {
    if (!WindowProxy.getInstance().onLine) {
      this.status_ = WallpaperSearchStatus.kOffline;
      recordStatusChange(this.status_);
      return;
    }

    const announcer = getAnnouncerInstance() as CrA11yAnnouncerElement;
    recordCustomizeChromeAction(
        CustomizeChromeAction.WALLPAPER_SEARCH_PROMPT_SUBMITTED);

    assert(this.descriptors_);
    const selectedDescriptorA = this.selectedDescriptorA_ ||
        getRandomDescriptorA(this.descriptors_.descriptorA);
    this.expandCategoryForDescriptorA_(selectedDescriptorA);
    this.selectedDescriptorA_ = selectedDescriptorA;
    this.loading_ = true;
    this.results_ = [];
    this.emptyResultContainers_ = [];
    announcer.announce(this.i18n('wallpaperSearchLoadingA11yMessage'));
    const descriptors: ResultDescriptors = {
      subject: this.selectedDescriptorA_!,
      style: this.selectedDescriptorB_ ?? undefined,
      mood: this.selectedDescriptorC_ ?? undefined,
      color: this.selectedDescriptorD_ ?? undefined,
    };
    this.resultsPromises_.push(
        this.wallpaperSearchHandler_.getWallpaperSearchResults(descriptors));
    if (this.resultsPromises_.length <= 1) {
      // Start processing requests, as well as any requests that are added
      // while waiting for results.
      while (this.resultsPromises_.length > 0) {
        const {status, results} = await this.resultsPromises_[0];
        this.resultsPromises_.shift();
        // The results of the last request to be processed will be shown in the
        // renderer.
        if (this.resultsPromises_.length === 0) {
          this.loading_ = false;
          this.results_ = results;
          this.resultsDescriptors_ = descriptors;
          this.status_ = status;
          if (this.status_ === WallpaperSearchStatus.kOk) {
            announcer.announce(
                this.i18n('wallpaperSearchSuccessA11yMessage', results.length));
          }
          recordStatusChange(status);
          this.selectedFeedbackOption_ = CrFeedbackOption.UNSPECIFIED;
          this.emptyResultContainers_ = this.calculateEmptyTiles(results);
        }
      }
    } else {
      // There are requests being processed already. This request will be
      // processed along with those.
      return;
    }
  }

  private onResultsRender_() {
    this.wallpaperSearchHandler_.setResultRenderTime(
        this.results_.map(r => r.id), WindowProxy.getInstance().now());
  }

  private async onResultClick_(e: DomRepeatEvent<WallpaperSearchResult>) {
    assert(this.resultsDescriptors_);
    recordCustomizeChromeAction(
        CustomizeChromeAction.WALLPAPER_SEARCH_RESULT_IMAGE_SELECTED);
    this.wallpaperSearchHandler_.setBackgroundToWallpaperSearchResult(
        e.model.item.id, WindowProxy.getInstance().now(),
        this.resultsDescriptors_);
  }

  private onStatusChange_() {
    if (this.status_ === WallpaperSearchStatus.kOk) {
      this.$.wallpaperSearch.focus();
    } else {
      this.$.error.focus();
    }
  }

  private shouldShowDeleteSelectedHueButton_() {
    return this.selectedHue_ !== null;
  }

  private shouldShowFeedbackButtons_() {
    return !this.loading_ && this.results_.length > 0;
  }

  private shouldShowGrid_(): boolean {
    return this.results_.length > 0 || this.emptyResultContainers_.length > 0;
  }

  private shouldShowHistory_(): boolean {
    return this.history_.length > 0;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'customize-chrome-wallpaper-search': WallpaperSearchElement;
  }
}

customElements.define(WallpaperSearchElement.is, WallpaperSearchElement);
