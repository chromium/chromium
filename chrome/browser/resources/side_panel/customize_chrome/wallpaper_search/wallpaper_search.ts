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

import type {SpHeading} from 'chrome://customize-chrome-side-panel.top-chrome/shared/sp_heading.js';
import type {ThemeHueSliderDialogElement} from 'chrome://resources/cr_components/theme_color_picker/theme_hue_slider_dialog.js';
import type {CrA11yAnnouncerElement} from 'chrome://resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import {getInstance as getAnnouncerInstance} from 'chrome://resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import type {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrFeedbackButtonsElement} from 'chrome://resources/cr_elements/cr_feedback_buttons/cr_feedback_buttons.js';
import {CrFeedbackOption} from 'chrome://resources/cr_elements/cr_feedback_buttons/cr_feedback_buttons.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {hexColorToSkColor} from 'chrome://resources/js/color_utils.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {Token} from 'chrome://resources/mojo/mojo/public/mojom/base/token.mojom-webui.js';
import type {DomRepeatEvent} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {Debouncer, PolymerElement, timeOut} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CustomizeChromeAction, recordCustomizeChromeAction} from '../common.js';
import type {CustomizeChromePageCallbackRouter, CustomizeChromePageHandlerInterface, Theme} from '../customize_chrome.mojom-webui.js';
import {CustomizeChromeApiProxy} from '../customize_chrome_api_proxy.js';
import type {DescriptorA, DescriptorB, DescriptorDValue, Descriptors, Inspiration, InspirationGroup, ResultDescriptors, WallpaperSearchClientCallbackRouter, WallpaperSearchHandlerInterface, WallpaperSearchResult} from '../wallpaper_search.mojom-webui.js';
import {DescriptorDName, UserFeedback, WallpaperSearchStatus} from '../wallpaper_search.mojom-webui.js';
import {WindowProxy} from '../window_proxy.js';

import type {ComboboxGroup, ComboboxItem, CustomizeChromeCombobox} from './combobox/customize_chrome_combobox.js';
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
        computed:
            'computeErrorState_(status_, shouldShowHistory_, shouldShowInspiration_)',
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
      inspirationGroups_: Object,
      inspirationToggleIcon_: {
        type: String,
        computed: 'computeInspirationToggleIcon_(openInspirations_)',
      },
      openInspirations_: Boolean,
      resultsDescriptors_: Object,
      results_: Object,
      selectedFeedbackOption_: {
        type: Number,
        value: CrFeedbackOption.UNSPECIFIED,
      },
      selectedDescriptorA_: String,
      selectedDescriptorB_: String,
      selectedDescriptorC_: String,
      selectedDescriptorD_: Object,
      selectedHue_: {
        type: Number,
        value: null,
      },
      shouldShowHistory_: {
        type: Boolean,
        computed: 'computeShouldShowHistory_(history_)',
      },
      shouldShowInspiration_: {
        type: Boolean,
        computed: 'computeShouldShowInspiration_(inspirationGroups_)',
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
  private inspirationGroups_: InspirationGroup[]|null;
  private inspirationCardEnabled_: boolean;
  private inspirationToggleIcon_: string;
  private loading_: boolean;
  private openInspirations_: boolean|undefined = false;
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
  private shouldShowHistory_: boolean;
  private shouldShowInspiration_: boolean;
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
    if (this.inspirationCardEnabled_) {
      this.wallpaperSearchHandler_.getInspirations().then(
          ({inspirationGroups}) => {
            this.inspirationGroups_ = inspirationGroups;
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
              this.openInspirations_ = !this.shouldShowHistory_;
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

  private getInspirationDescriptorsCheckedStatus_(
      groupDescriptors: ResultDescriptors): string {
    const groupDescriptorColor = groupDescriptors.color?.name !== undefined ?
        descriptorDNameToHex(groupDescriptors.color!.name) :
        undefined;
    return (groupDescriptors.subject || null) === this.selectedDescriptorA_ &&
            (groupDescriptors.style || null) === this.selectedDescriptorB_ &&
            (groupDescriptors.mood || null) === this.selectedDescriptorC_ &&
            groupDescriptorColor === this.selectedDefaultColor_ ?
        'true' :
        'false';
  }

  private getInspirationGroupTitle_(descriptors: ResultDescriptors): string {
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
      descriptors.subject,
      descriptors.style,
      descriptors.mood,
      colorName,
    ].filter(Boolean)
        .join(', ');
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

  private onButtonKeydown_(e: KeyboardEvent) {
    if (['Enter', ' '].includes(e.key)) {
      e.preventDefault();
      e.stopPropagation();
      (e.target as HTMLElement).click();
    }
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
    if (this.selectedDefaultColor_ === e.model.item) {
      this.selectedDefaultColor_ = undefined;
      this.selectedDescriptorD_ = null;
    } else {
      this.selectedDefaultColor_ = e.model.item;
      this.selectedDescriptorD_ = {
        color: hexColorToSkColor(this.selectedDefaultColor_),
      };
    }
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
        e.model.item.id,
        e.model.item.descriptors ??
            {subject: null, style: null, mood: null, color: null});
  }

  private onInspirationGroupTitleClick_(e: DomRepeatEvent<InspirationGroup>) {
    this.selectDescriptorsFromInspirationGroup_(e.model.item);
  }

  private onInspirationToggleClick_() {
    this.openInspirations_ = !this.openInspirations_;
  }

  private onInspirationImageClick_(e: Event&{
    model: {
      item: Inspiration,
      parentModel: {
        item: InspirationGroup,
      },
    },
  }) {
    recordCustomizeChromeAction(
        CustomizeChromeAction.WALLPAPER_SEARCH_INSPIRATION_THEME_SELECTED);
    this.wallpaperSearchHandler_.setBackgroundToInspirationImage(
        e.model.item.id, e.model.item.backgroundUrl);
    this.selectDescriptorsFromInspirationGroup_(e.model.parentModel.item);
  }

  private onLearnMoreClick_(e: Event) {
    e.preventDefault();
    this.wallpaperSearchHandler_.openHelpArticle();
  }

  private onSelectedHueChanged_() {
    this.selectedDefaultColor_ = undefined;
    this.selectedHue_ = this.$.hueSlider.selectedHue;
    this.selectedDescriptorD_ = {hue: this.selectedHue_};
    recordCustomizeChromeAction(
        CustomizeChromeAction.WALLPAPER_SEARCH_COLOR_DESCRIPTOR_UPDATED);
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

  private selectDescriptorsFromInspirationGroup_(group: InspirationGroup) {
    const announcer = getAnnouncerInstance() as CrA11yAnnouncerElement;
    const groupDescriptors = group.descriptors;
    this.selectedDescriptorA_ = groupDescriptors.subject || null;
    this.selectedDescriptorB_ = groupDescriptors.style || null;
    this.selectedDescriptorC_ = groupDescriptors.mood || null;

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

  private shouldShowDeleteSelectedHueButton_() {
    return this.selectedHue_ !== null;
  }

  private shouldShowFeedbackButtons_() {
    return !this.loading_ && this.results_.length > 0;
  }

  private shouldShowGrid_(): boolean {
    return this.results_.length > 0 || this.emptyResultContainers_.length > 0;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'customize-chrome-wallpaper-search': WallpaperSearchElement;
  }
}

customElements.define(WallpaperSearchElement.is, WallpaperSearchElement);
