// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../check_mark_wrapper.js';
import './combobox/customize_chrome_combobox.js';
import 'chrome://customize-chrome-side-panel.top-chrome/shared/sp_heading.js';
import 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';
import 'chrome://resources/cr_elements/cr_feedback_buttons/cr_feedback_buttons.js';
import 'chrome://resources/cr_elements/cr_grid/cr_grid.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_loading_gradient/cr_loading_gradient.js';
import 'chrome://resources/cr_elements/cr_ripple/cr_ripple.js';
import 'chrome://resources/cr_components/theme_color_picker/theme_hue_slider_dialog.js';

import type {SpHeadingElement} from 'chrome://customize-chrome-side-panel.top-chrome/shared/sp_heading.js';
import type {ThemeHueSliderDialogElement} from 'chrome://resources/cr_components/theme_color_picker/theme_hue_slider_dialog.js';
import type {CrA11yAnnouncerElement} from 'chrome://resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import {getInstance as getAnnouncerInstance} from 'chrome://resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrFeedbackButtonsElement} from 'chrome://resources/cr_elements/cr_feedback_buttons/cr_feedback_buttons.js';
import {CrFeedbackOption} from 'chrome://resources/cr_elements/cr_feedback_buttons/cr_feedback_buttons.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {hexColorToSkColor} from 'chrome://resources/js/color_utils.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {Token} from 'chrome://resources/mojo/mojo/public/mojom/base/token.mojom-webui.js';

import {CustomizeChromeAction, recordCustomizeChromeAction} from '../common.js';
import type {CustomizeChromePageCallbackRouter, CustomizeChromePageHandlerInterface, Theme} from '../customize_chrome.mojom-webui.js';
import {CustomizeChromeApiProxy} from '../customize_chrome_api_proxy.js';
import type {DescriptorB, DescriptorDValue, Descriptors, Group, InspirationDescriptors, InspirationGroup, ResultDescriptors, WallpaperSearchClientCallbackRouter, WallpaperSearchHandlerInterface, WallpaperSearchResult} from '../wallpaper_search.mojom-webui.js';
import {DescriptorDName, UserFeedback, WallpaperSearchStatus} from '../wallpaper_search.mojom-webui.js';
import {WindowProxy} from '../window_proxy.js';

import type {ComboboxGroup, ComboboxItem, CustomizeChromeComboboxElement} from './combobox/customize_chrome_combobox.js';
import {getCss} from './wallpaper_search.css.js';
import {getHtml} from './wallpaper_search.html.js';
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

function descriptorDNameToHex(name: DescriptorDName): string {
  switch (name) {
    case DescriptorDName.kYellow:
      return '#f9cc18';
  }
}

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

export interface WallpaperSearchResponse {
  status: WallpaperSearchStatus;
  results: WallpaperSearchResult[];
}

export interface WallpaperSearchElement {
  $: {
    customColorContainer: HTMLElement,
    deleteSelectedHueButton: HTMLElement,
    descriptorComboboxA: CustomizeChromeComboboxElement,
    descriptorComboboxB: CustomizeChromeComboboxElement,
    descriptorComboboxC: CustomizeChromeComboboxElement,
    feedbackButtons: CrFeedbackButtonsElement,
    heading: SpHeadingElement,
    historyCard: HTMLElement,
    hueSlider: ThemeHueSliderDialogElement,
    loading: HTMLElement,
    submitButton: CrButtonElement,
    wallpaperSearch: HTMLElement,
  };
}

function getRandomDescriptorA(groups: Group[]): string {
  const descriptorAs =
      groups[Math.floor(Math.random() * groups.length)]!.descriptorAs;
  return descriptorAs[Math.floor(Math.random() * descriptorAs.length)]!.key;
}

function recordStatusChange(status: WallpaperSearchStatus) {
  chrome.metricsPrivate.recordEnumerationValue(
      'NewTabPage.WallpaperSearch.Status', status,
      WallpaperSearchStatus.MAX_VALUE);
}

function getEventTargetIndex(e: Event): number {
  return Number((e.currentTarget as HTMLElement).dataset['index']);
}

const WallpaperSearchElementBase = I18nMixinLit(CrLitElement);

export class WallpaperSearchElement extends WallpaperSearchElementBase {
  static get is() {
    return 'customize-chrome-wallpaper-search';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      comboboxItems_: {type: Object},
      descriptors_: {type: Object},
      descriptorD_: {type: Array},
      errorState_: {type: Object},
      emptyHistoryContainers_: {type: Object},
      emptyResultContainers_: {type: Object},
      expandedCategories_: {type: Object},
      loading_: {type: Boolean},
      history_: {type: Array},
      inspirationCardEnabled_: {type: Boolean},
      inspirationGroups_: {type: Array},
      inspirationToggleIcon_: {type: String},
      openInspirations_: {type: Boolean},
      resultsDescriptors_: {type: Object},
      results_: {type: Object},
      selectedFeedbackOption_: {type: Number},
      selectedDescriptorA_: {type: String},
      selectedDescriptorB_: {type: String},
      selectedDescriptorC_: {type: String},
      selectedDescriptorD_: {type: Object},
      selectedHue_: {type: Number},
      shouldShowHistory_: {type: Boolean},
      shouldShowInspiration_: {type: Boolean},
      status_: {type: Number},
      theme_: {type: Object},
    };
  }

  protected comboboxItems_: ComboxItems = {
    a: [],
    b: [],
    c: [],
  };
  private descriptors_: Descriptors|null = null;
  protected descriptorD_: string[] = DESCRIPTOR_D_VALUE.map(value => value.hex);
  protected emptyHistoryContainers_: number[] = [];
  protected emptyResultContainers_: number[] = [];
  private errorCallback_: (() => void)|undefined;
  protected errorState_: ErrorState|null = null;
  private expandedCategories_: {[categoryIndex: number]: boolean} = {};
  protected history_: WallpaperSearchResult[] = [];
  protected inspirationGroups_: InspirationGroup[] = [];
  protected inspirationCardEnabled_: boolean =
      loadTimeData.getBoolean('wallpaperSearchInspirationCardEnabled');
  protected inspirationToggleIcon_: string = '';
  protected loading_: boolean = false;
  protected openInspirations_: boolean = false;
  protected results_: WallpaperSearchResult[] = [];
  private resultsDescriptors_: ResultDescriptors|null = null;
  private resultsPromises_: Array<Promise<WallpaperSearchResponse>> = [];
  private selectedDefaultColor_: string|undefined;
  protected selectedDescriptorA_: string|null = null;
  protected selectedDescriptorB_: string|null = null;
  protected selectedDescriptorC_: string|null = null;
  private selectedDescriptorD_: DescriptorDValue|null = null;
  protected selectedFeedbackOption_: CrFeedbackOption =
      CrFeedbackOption.UNSPECIFIED;
  protected selectedHue_: number|null = null;
  protected shouldShowHistory_: boolean = false;
  protected shouldShowInspiration_: boolean = false;
  private status_: WallpaperSearchStatus = WallpaperSearchStatus.kOk;
  private theme_?: Theme;

  private callbackRouter_: CustomizeChromePageCallbackRouter;
  private pageHandler_: CustomizeChromePageHandlerInterface;
  private wallpaperSearchCallbackRouter_: WallpaperSearchClientCallbackRouter;
  private wallpaperSearchHandler_: WallpaperSearchHandlerInterface;
  private setThemeListenerId_: number|null = null;
  private setHistoryListenerId_: number|null = null;
  private loadingUiResizeObserver_: ResizeObserver|null = null;

  constructor() {
    super();
    this.callbackRouter_ = CustomizeChromeApiProxy.getInstance().callbackRouter;
    this.pageHandler_ = CustomizeChromeApiProxy.getInstance().handler;
    this.wallpaperSearchHandler_ = WallpaperSearchProxy.getInstance().handler;
    this.wallpaperSearchCallbackRouter_ =
        WallpaperSearchProxy.getInstance().callbackRouter;
    this.fetchDescriptors_();
    if (this.inspirationCardEnabled_) {
      this.wallpaperSearchHandler_.getInspirations().then(
          ({inspirationGroups}) => {
            this.inspirationGroups_ = inspirationGroups || [];
          });
    }
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
              this.openInspirations_ = !this.computeShouldShowHistory_();
            });
    this.wallpaperSearchHandler_.updateHistory();
    this.loadingUiResizeObserver_ = new ResizeObserver(() => {
      this.generateLoadingUi_();
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

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    this.shouldShowInspiration_ = this.computeShouldShowInspiration_();
    this.inspirationToggleIcon_ = this.computeInspirationToggleIcon_();
    this.shouldShowHistory_ = this.computeShouldShowHistory_();
    this.errorState_ = this.computeErrorState_();
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedPrivateProperties.has('status_')) {
      this.onStatusChange_();
    }
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
        let errorDescription;
        if (this.shouldShowHistory_ && this.shouldShowInspiration_) {
          errorDescription =
              this.i18n('genericErrorDescriptionWithHistoryAndInspiration');
        } else if (this.shouldShowHistory_) {
          errorDescription = this.i18n('genericErrorDescriptionWithHistory');
        } else if (this.shouldShowInspiration_) {
          errorDescription =
              this.i18n('genericErrorDescriptionWithInspiration');
        } else {
          errorDescription = this.i18n('genericErrorDescription');
        }
        return {
          title: this.i18n('genericErrorTitle'),
          description: errorDescription,
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
          description: this.shouldShowHistory_ ?
              this.i18n('offlineDescriptionWithHistory') :
              this.i18n('offlineDescription'),
          callToAction: this.i18n('ok'),
        };
      case WallpaperSearchStatus.kSignedOut:
        return {
          title: this.i18n('signedOutTitle'),
          description: this.i18n('signedOutDescription'),
          callToAction: this.i18n('ok'),
        };
    }
  }

  private computeInspirationToggleIcon_(): string {
    return this.openInspirations_ ? 'collapse-carets' : 'expand-carets';
  }

  private computeShouldShowHistory_(): boolean {
    return this.history_.length > 0;
  }

  private computeShouldShowInspiration_(): boolean {
    return !!this.inspirationGroups_ && this.inspirationGroups_.length > 0;
  }

  private expandCategoryForDescriptorA_(key: string) {
    if (!this.descriptors_) {
      return;
    }
    const categoryGroupIndex = this.descriptors_.groups.findIndex(
        group => group.descriptorAs.some(descriptor => descriptor.key === key));
    if (categoryGroupIndex >= 0) {
      this.expandedCategories_[categoryGroupIndex] = true;
      this.requestUpdate();
    }
  }

  private async fetchDescriptors_() {
    this.wallpaperSearchHandler_.getDescriptors().then(({descriptors}) => {
      if (descriptors) {
        // Order the descriptors so they appear alphabetically in all languages.
        descriptors.groups.sort((a, b) => a.category.localeCompare(b.category));
        descriptors.groups.forEach(
            (group) => group.descriptorAs.sort(
                (a, b) => a.label.localeCompare(b.label)));
        descriptors.descriptorB.sort((a, b) => a.label.localeCompare(b.label));
        descriptors.descriptorC.sort((a, b) => a.label.localeCompare(b.label));

        this.descriptors_ = descriptors;
        this.comboboxItems_ = {
          a: descriptors.groups.map((group) => {
            return {
              key: group.category,
              label: group.category,
              items: group.descriptorAs,
            };
          }),
          b: descriptors.descriptorB,
          c: descriptors.descriptorC,
        };
        this.errorCallback_ = undefined;
        recordStatusChange(WallpaperSearchStatus.kOk);
      } else {
        // Wallpaper search cannot render properly without descriptors, so the
        // error callback takes the user back a page.
        this.errorCallback_ = () => this.dispatchEvent(new Event('back-click'));
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

  protected getColorLabel_(defaultColor: string): string {
    const descriptor =
        DESCRIPTOR_D_VALUE.find((color) => color.hex === defaultColor);
    return descriptor ? loadTimeData.getString(descriptor.name) : '';
  }

  protected isCustomColorSelected_(): boolean {
    return this.selectedHue_ !== null;
  }

  protected getInspirationDescriptorsCheckedStatus_(
      groupDescriptors: InspirationDescriptors): string {
    const groupDescriptorColor = groupDescriptors.color?.name !== undefined ?
        descriptorDNameToHex(groupDescriptors.color!.name) :
        undefined;
    return (groupDescriptors.subject?.key || null) ===
                this.selectedDescriptorA_ &&
            (groupDescriptors.style?.key || null) ===
                this.selectedDescriptorB_ &&
            (groupDescriptors.mood?.key || null) ===
                this.selectedDescriptorC_ &&
            groupDescriptorColor === this.selectedDefaultColor_ ?
        'true' :
        'false';
  }

  protected getInspirationGroupTitle_(descriptors: InspirationDescriptors):
      string {
    // Filter out undefined or null values, then join the rest into a comma
    // separated string.
    let colorName;
    if (descriptors.color?.name !== undefined) {
      const hex = descriptorDNameToHex(descriptors.color.name);
      if (hex) {
        colorName = this.getColorLabel_(hex);
      }
    }
    return [
      descriptors.subject?.label,
      descriptors.style?.label,
      descriptors.mood?.label,
      colorName,
    ].filter(Boolean)
        .join(this.i18n('separator'));
  }

  protected getHistoryResultAriaLabel_(
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

  protected getResultAriaLabel_(index: number): string {
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

  protected isBackgroundSelected_(id: Token): boolean {
    return !!(
        this.theme_ && this.theme_.backgroundImage &&
        this.theme_.backgroundImage.localBackgroundId &&
        this.theme_.backgroundImage.localBackgroundId.low === id.low &&
        this.theme_.backgroundImage.localBackgroundId.high === id.high);
  }

  protected isColorSelected_(defaultColor: string): boolean {
    return defaultColor === this.selectedDefaultColor_;
  }

  protected isOptionSelectedInDescriptorB_(option: DescriptorB): boolean {
    return option.key === this.selectedDescriptorB_;
  }

  protected onBackClick_() {
    this.dispatchEvent(new Event('back-click'));
  }

  protected onButtonKeydown_(e: KeyboardEvent) {
    if (['Enter', ' '].includes(e.key)) {
      e.preventDefault();
      e.stopPropagation();
      (e.target as HTMLElement).click();
    }
  }

  protected onComboboxCategoryClick_(e: Event) {
    const index = getEventTargetIndex(e);
    const previous = this.expandedCategories_[index];
    this.expandedCategories_[index] = !previous;
    this.requestUpdate();
  }

  protected onCustomColorClick_() {
    this.$.hueSlider.showAt(this.$.customColorContainer);
  }

  protected onErrorClick_() {
    this.status_ = WallpaperSearchStatus.kOk;
    recordStatusChange(this.status_);
    if (this.errorCallback_) {
      this.errorCallback_();
    }
  }

  protected onDefaultColorClick_(e: Event) {
    const index = getEventTargetIndex(e);
    const item = this.descriptorD_[index]!;
    this.selectedHue_ = null;
    if (this.selectedDefaultColor_ === item) {
      this.selectedDefaultColor_ = undefined;
      this.selectedDescriptorD_ = null;
    } else {
      this.selectedDefaultColor_ = item;
      this.selectedDescriptorD_ = {
        color: hexColorToSkColor(this.selectedDefaultColor_),
      };
    }
    recordCustomizeChromeAction(
        CustomizeChromeAction.WALLPAPER_SEARCH_COLOR_DESCRIPTOR_UPDATED);
  }

  protected onMoodDescriptorChange_(e: CustomEvent<{value: string}>) {
    if (this.selectedDescriptorC_ !== e.detail.value) {
      recordCustomizeChromeAction(
          CustomizeChromeAction.WALLPAPER_SEARCH_MOOD_DESCRIPTOR_UPDATED);
    }
    this.selectedDescriptorC_ = e.detail.value;
  }

  protected onStyleDescriptorChange_(e: CustomEvent<{value: string}>) {
    if (this.selectedDescriptorB_ !== e.detail.value) {
      recordCustomizeChromeAction(
          CustomizeChromeAction.WALLPAPER_SEARCH_STYLE_DESCRIPTOR_UPDATED);
    }
    this.selectedDescriptorB_ = e.detail.value;
  }

  protected onSubjectDescriptorChange_(e: CustomEvent<{value: string}>) {
    if (this.selectedDescriptorA_ !== e.detail.value) {
      recordCustomizeChromeAction(
          CustomizeChromeAction.WALLPAPER_SEARCH_SUBJECT_DESCRIPTOR_UPDATED);
    }
    this.selectedDescriptorA_ = e.detail.value;
  }

  protected onFeedbackSelectedOptionChanged_(
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

  protected onHistoryImageClick_(e: Event) {
    const index = getEventTargetIndex(e);
    const item = this.history_[index]!;
    recordCustomizeChromeAction(
        CustomizeChromeAction.WALLPAPER_SEARCH_HISTORY_IMAGE_SELECTED);
    this.wallpaperSearchHandler_.setBackgroundToHistoryImage(
        item.id,
        item.descriptors ??
            {subject: null, style: null, mood: null, color: null});
  }

  protected onInspirationGroupTitleClick_(e: Event) {
    assert(this.inspirationGroups_);
    const index = getEventTargetIndex(e);
    const item = this.inspirationGroups_[index]!;
    this.selectDescriptorsFromInspirationGroup_(item);
  }

  protected onInspirationToggleClick_() {
    this.openInspirations_ = !this.openInspirations_;
  }

  protected onInspirationImageClick_(e: Event) {
    const groupIndex =
        Number((e.currentTarget as HTMLElement).dataset['groupIndex']);
    const inspirationGroup = this.inspirationGroups_[groupIndex]!;
    const index = getEventTargetIndex(e);
    const item = inspirationGroup.inspirations[index]!;
    recordCustomizeChromeAction(
        CustomizeChromeAction.WALLPAPER_SEARCH_INSPIRATION_THEME_SELECTED);
    this.wallpaperSearchHandler_.setBackgroundToInspirationImage(
        item.id, item.backgroundUrl);
    this.selectDescriptorsFromInspirationGroup_(inspirationGroup);
  }

  protected onLearnMoreClick_(e: Event) {
    e.preventDefault();
    this.wallpaperSearchHandler_.openHelpArticle();
  }

  protected onSelectedHueChanged_() {
    this.selectedDefaultColor_ = undefined;
    this.selectedHue_ = this.$.hueSlider.selectedHue;
    this.selectedDescriptorD_ = {hue: this.selectedHue_};
    recordCustomizeChromeAction(
        CustomizeChromeAction.WALLPAPER_SEARCH_COLOR_DESCRIPTOR_UPDATED);
  }

  protected onSelectedHueDelete_() {
    this.selectedHue_ = null;
    this.selectedDescriptorD_ = null;
    this.$.hueSlider.hide();
    this.$.customColorContainer.focus();
  }

  protected async onSearchClick_() {
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
        getRandomDescriptorA(this.descriptors_.groups);
    this.expandCategoryForDescriptorA_(selectedDescriptorA);
    this.selectedDescriptorA_ = selectedDescriptorA;
    this.loading_ = true;
    this.results_ = [];
    this.emptyResultContainers_ = [];
    announcer.announce(this.i18n('wallpaperSearchLoadingA11yMessage'));
    const descriptors: ResultDescriptors = {
      subject: this.selectedDescriptorA_!,
      style: this.selectedDescriptorB_ ?? null,
      mood: this.selectedDescriptorC_ ?? null,
      color: this.selectedDescriptorD_ ?? null,
    };
    this.resultsPromises_.push(
        this.wallpaperSearchHandler_.getWallpaperSearchResults(descriptors));
    if (this.resultsPromises_.length <= 1) {
      // Start processing requests, as well as any requests that are added
      // while waiting for results.
      while (this.resultsPromises_.length > 0) {
        const {status, results} = await this.resultsPromises_[0]!;
        this.resultsPromises_.shift();
        // The results of the last request to be processed will be shown in the
        // renderer.
        if (this.resultsPromises_.length === 0) {
          this.loading_ = false;
          this.results_ = results;
          this.updateComplete.then(this.onResultsRender_.bind(this));
          this.resultsDescriptors_ = descriptors;
          this.status_ = status;
          if (this.status_ === WallpaperSearchStatus.kOk) {
            announcer.announce(
                this.i18n('wallpaperSearchSuccessA11yMessage', results.length));
            this.wallpaperSearchHandler_.launchHatsSurvey();
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

  protected onResultClick_(e: Event) {
    const index = getEventTargetIndex(e);
    const item = this.results_[index]!;
    assert(this.resultsDescriptors_);
    recordCustomizeChromeAction(
        CustomizeChromeAction.WALLPAPER_SEARCH_RESULT_IMAGE_SELECTED);
    this.wallpaperSearchHandler_.setBackgroundToWallpaperSearchResult(
        item.id, WindowProxy.getInstance().now(), this.resultsDescriptors_);
  }

  private onStatusChange_() {
    if (this.status_ === WallpaperSearchStatus.kOk) {
      this.$.wallpaperSearch.focus();
    } else {
      const error = this.shadowRoot!.querySelector<HTMLElement>('#error');
      assert(error);
      error.focus();
    }
  }

  private selectDescriptorsFromInspirationGroup_(group: InspirationGroup) {
    const announcer = getAnnouncerInstance() as CrA11yAnnouncerElement;
    const groupDescriptors = group.descriptors;
    this.selectedDescriptorA_ = groupDescriptors.subject?.key || null;
    this.selectedDescriptorB_ = groupDescriptors.style?.key || null;
    this.selectedDescriptorC_ = groupDescriptors.mood?.key || null;

    if (groupDescriptors.color?.name !== undefined) {
      const hex = descriptorDNameToHex(groupDescriptors.color.name);
      this.selectedDefaultColor_ = hex;
      this.selectedHue_ = null;
      this.selectedDescriptorD_ = {
        color: hexColorToSkColor(this.selectedDefaultColor_),
      };
    } else {
      this.selectedDefaultColor_ = undefined;
      this.selectedHue_ = null;
      this.selectedDescriptorD_ = null;
    }
    announcer.announce(
        this.i18n('wallpaperSearchDescriptorsChangedA11yMessage'));
  }

  protected shouldShowDeleteSelectedHueButton_() {
    return this.selectedHue_ !== null;
  }

  protected shouldShowFeedbackButtons_() {
    return !this.loading_ && this.results_.length > 0;
  }

  protected shouldShowGrid_(): boolean {
    return this.results_.length > 0 || this.emptyResultContainers_.length > 0;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'customize-chrome-wallpaper-search': WallpaperSearchElement;
  }
}

customElements.define(WallpaperSearchElement.is, WallpaperSearchElement);
