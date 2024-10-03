// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './icons.html.js';
import './emoji_image.js';
import './emoji_gif_nudge_overlay.js';
import './emoji_group.js';
import './emoji_group_button.js';
import './emoji_search.js';
import './emoji_error.js';
import './emoji_category_button.js';
import './text_group_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_auto_img/cr_auto_img.js';
import 'chrome://resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_icons.css.js';

import {getInstance as getAnnouncerInstance} from '//resources/ash/common/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import {CrIconButtonElement} from '//resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';
import {afterNextRender, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';
import * as constants from './constants.js';
import {EmojiGroupComponent} from './emoji_group.js';
import {Category, Feature} from './emoji_picker.mojom-webui.js';
import {EmojiPickerApiProxy} from './emoji_picker_api_proxy.js';
import {EmojiSearch} from './emoji_search.js';
import * as events from './events.js';
import {CATEGORY_METADATA, CATEGORY_TABS, EMOJI_GROUP_TABS, GIF_CATEGORY_METADATA, gifCategoryTabs, SUBCATEGORY_TABS, TABS_CATEGORY_START_INDEX, TABS_CATEGORY_START_INDEX_GIF_SUPPORT} from './metadata_extension.js';
import {EmojiPreferencesStore, GifNudgeHistoryStore, RecentlyUsedStore} from './store.js';
import {Status} from './tenor_types.mojom-webui.js';
import {CategoryEnum, Emoji, EmojiGroupData, EmojiGroupElement, EmojiVariants, Gender, GifSubcategoryData, PreferenceMapping, SubcategoryData, Tone} from './types.js';

export interface EmojiPickerApp {
  $: {
    'left-chevron': CrIconButtonElement,
    'list-container': HTMLDivElement,
    'right-chevron': CrIconButtonElement,
    'search-container': EmojiSearch,
    bar: HTMLDivElement,
    dummyTab: HTMLDivElement,
    groups: HTMLDivElement,
    message: HTMLDivElement,
    tabs: HTMLDivElement,
  };
}


export class EmojiPickerApp extends PolymerElement {
  static get is() {
    return 'emoji-picker-app' as const;
  }

  static get template() {
    return getTemplate();
  }

  static configs() {
    return {
      'dataUrls': {
        [CategoryEnum.EMOJI]: [
          '/emoji_15_0_ordering_start.json',
          '/emoji_15_0_ordering_remaining.json',
        ],
        [CategoryEnum.EMOTICON]: ['/emoticon_ordering.json'],
        [CategoryEnum.SYMBOL]: ['/symbol_ordering.json'],
        // GIFs are not preloaded hence the empty data url.
        [CategoryEnum.GIF]: [''],
      },
    };
  }

  static get properties() {
    return {
      category: {type: String, value: 'emoji', observer: 'onCategoryChanged'},
      emojiGroupTabs: {type: Array},
      dummyTab: {
        type: Object,
        value: () => ({
          name: '',
          groupId: '-1',
          active: false,
          disabled: false,
          category: CategoryEnum.GIF,
        }),
      },
      categoriesData: {type: Array, value: () => ([])},
      categoriesGroupElements: {type: Array, value: () => ([])},
      activeInfiniteGroupId: {type: String, value: null},
      categoriesHistory: {type: Object, value: () => ({})},
      globalTone: {type: Number, value: null},
      globalGender: {type: Number, value: null},
      pagination: {type: Number, value: 1, observer: 'onPaginationChanged'},
      searchLazyIndexing: {type: Boolean, value: true},
      textSubcategoryBarEnabled: {
        type: Boolean,
        value: false,
        computed: 'isTextSubcategoryBarEnabled(category)',
        reflectToAttribute: true,
      },
      incognito: {type: Boolean, value: true},
      gifSupport: {type: Boolean, value: false},
      sealSupport: {type: Boolean, value: false},
      variantGroupingSupport: {type: Boolean, value: false},
      showGifNudgeOverlay: {type: Boolean, value: false},
      nextGifPos: {type: Object, value: () => ({})},
      status: {type: Status, value: null},
      errorMessage: {type: String, value: constants.NO_INTERNET_VIEW_ERROR_MSG},
      useMojoSearch: {type: Boolean, value: false},
    };
  }
  private category: CategoryEnum;
  private emojiGroupTabs: SubcategoryData[] = EMOJI_GROUP_TABS;
  private dummyTab: SubcategoryData;
  private allCategoryTabs: SubcategoryData[] = SUBCATEGORY_TABS;
  categoriesData: EmojiGroupData;
  categoriesGroupElements: EmojiGroupElement[];
  activeInfiniteGroupId: string|null; // null before Trending GIFs are fetched
  private categoriesHistory: {[index in CategoryEnum]: RecentlyUsedStore|null};
  private emojiPreferences: EmojiPreferencesStore|null = null;
  private globalTone: Tone|null = null;
  private globalGender: Gender|null = null;
  private pagination: number;
  private searchLazyIndexing: boolean;
  private textSubcategoryBarEnabled: boolean;
  private incognito: boolean;
  private gifSupport: boolean;
  private sealSupport: boolean;
  private variantGroupingSupport: boolean;
  private showGifNudgeOverlay: boolean;
  private activeVariant: EmojiGroupComponent|null = null;
  private apiProxy: EmojiPickerApiProxy = EmojiPickerApiProxy.getInstance();
  private autoScrollingToGroup: boolean = false;
  private highlightBarMoving: boolean = false;
  private nextGifPos: {[key: string]: string};
  private status: Status|null;
  private previousGifValidation: Date;
  private fetchAndProcessDataPromise: Promise<void>|null;
  private useMojoSearch = false;

  constructor() {
    super();

    // Incognito mode is set based on the default value.
    this.updateIncognitoState(this.incognito);

    this.previousGifValidation = this.loadPreviousGifValidationTime();

    this.addEventListener(
        events.GROUP_BUTTON_CLICK,
        (ev: events.GroupButtonClickEvent) =>
            this.selectGroup(ev.detail.group));
    this.addEventListener(
        events.EMOJI_TEXT_BUTTON_CLICK,
        (ev: events.EmojiTextButtonClickEvent) =>
            this.onEmojiTextButtonClick(ev));
    this.addEventListener(
        events.EMOJI_IMG_BUTTON_CLICK,
        (ev: events.EmojiImgButtonClickEvent) =>
            this.onEmojiImgButtonClick(ev));
    this.addEventListener(
        events.EMOJI_CLEAR_RECENTS_CLICK,
        (ev: events.EmojiClearRecentClickEvent) => this.clearRecentEmoji(ev));
    // variant popup related handlers
    this.addEventListener(
        events.EMOJI_VARIANTS_SHOWN,
        (ev: events.EmojiVariantsShownEvent) => this.onShowEmojiVariants(ev));
    this.addEventListener('click', () => this.hideDialogs());
    this.addEventListener(
        events.CATEGORY_BUTTON_CLICK,
        (ev: events.CategoryButtonClickEvent) =>
            this.onCategoryButtonClick(ev.detail.categoryName));
    this.addEventListener(
        'search',
        ev => this.onSearchChanged((ev as CustomEvent<string>).detail));
    this.addEventListener(events.GIF_ERROR_TRY_AGAIN, this.onClickTryAgain);

    // This function will be passed down to some child element, thus we need
    // `bind(this)`.
    this.closeGifNudgeOverlay = this.closeGifNudgeOverlay.bind(this);
  }

  private filterGroupTabByPagination(pageNumber: number): (tab: {
    pagination: number,
    groupId: string,
  }) => boolean {
    return function(tab: {pagination: number, groupId: string}) {
      return tab.pagination === pageNumber && !tab.groupId.includes('history');
    };
  }

  private async initHistoryUi(incognito: boolean) {
    if (incognito !== this.incognito) {
      await this.updateIncognitoState(incognito);
    }
    this.updateHistoryTabDisabledProperty();
    // Make highlight bar visible (now we know where it should be) and
    // add smooth sliding.
    this.updateActiveGroup();
    this.$.bar.style.display = 'block';
    this.$.bar.style.transition = 'left 200ms';
  }

  override ready() {
    super.ready();

    const METADATA =
        this.gifSupport ? GIF_CATEGORY_METADATA : CATEGORY_METADATA;

    // Ensure first category is emoji for compatibility with V1.
    if (METADATA[0]?.name !== CategoryEnum.EMOJI) {
      throw new Error(
          `First category is ${METADATA[0]?.name} but must be 'emoji'.`);
    }

    const dataUrls = EmojiPickerApp.configs().dataUrls;
    // Create an ordered list of category and urls based on the order that
    // categories need to appear in the UIs.
    const categoryDataUrls =
        METADATA.filter((item) => dataUrls[item.name])
            .map(
                item => ({'category': item.name, 'urls': dataUrls[item.name]}));

    // Fetch and process all the data.
    this.fetchAndProcessDataPromise = this.fetchAndProcessData(categoryDataUrls);

    this.updateStyles({
      '--emoji-group-button-size': constants.EMOJI_GROUP_SIZE_PX,
      '--emoji-picker-width': constants.EMOJI_PICKER_WIDTH_PX,
      '--emoji-picker-height': constants.EMOJI_PICKER_HEIGHT_PX,
      '--emoji-size': constants.EMOJI_SIZE_PX,
      '--emoji-per-row': constants.EMOJI_PER_ROW,
      '--emoji-picker-search-side-padding':
          constants.EMOJI_PICKER_SIDE_PADDING_PX,
      '--emoji-picker-side-padding': constants.EMOJI_PICKER_SIDE_PADDING_PX,
      '--emoji-picker-top-padding': constants.EMOJI_PICKER_TOP_PADDING_PX,
      '--emoji-spacing': constants.EMOJI_SPACING_PX,
      '--emoji-group-spacing': constants.EMOJI_GROUP_SPACING_PX,
      '--visual-content-padding': constants.VISUAL_CONTENT_PADDING_PX,
      '--visual-content-width': constants.VISUAL_CONTENT_WIDTH_PX,
      '--tab-button-margin': constants.TAB_BUTTON_MARGIN_PX,
      '--text-group-button-padding': constants.TEXT_GROUP_BUTTON_PADDING_PX,
    });
  }

  private async ensureFetchAndProcessDataFinished(): Promise<void> {
    if (this.fetchAndProcessDataPromise !== null) {
      await this.fetchAndProcessDataPromise;
    }
  }

  /**
   * Fetches data and updates all the variables that are required to render
   * EmojiPicker UI. This function serves as the main entry for creating and
   * managing async calls dealing with fetching data and rendering UI in the
   * correct order. These include:
   *   * Feature list
   *   * Incognito state
   *   * Category data (emoji, emoticon, etc.)
   */
  private async fetchAndProcessData(
      categoryDataUrls: Array<{category: CategoryEnum, urls: string[]}>) {
    // Create a flat list of urls (with details) that need to be fetched and
    // rendered sequentially.
    const dataUrls = categoryDataUrls.flatMap(
        item =>
            // Create url details of the category.
        item.urls.map(
            (url, index) => ({
              'category': item.category,
              'url': url,
              'categoryLastPartition': index === item.urls.length - 1,
            }),
            ),
    );

    const firstResult = dataUrls[0];
    if (!firstResult) {
      return;
    }

    // Update feature list, incognito state and fetch data of first url.
    const initialData =
        await Promise
            .all(
                [
                  this.fetchOrderingData(firstResult.url),
                  this.apiProxy.getFeatureList().then(
                      (response: {featureList: number[]}) =>
                          this.setActiveFeatures(response.featureList)),
                  this.apiProxy.isIncognitoTextField().then(
                      (response: {incognito: boolean}) =>
                          this.initHistoryUi(response.incognito)),
                ],
                )
            .then(values => values[0]);  // Map to the fetched data only.

    // After initial data is loaded, if the GIF nudge is not shown before, show
    // the GIF nudge.
    if (this.gifSupport && !GifNudgeHistoryStore.hasNudgeShown()) {
      this.showGifNudgeOverlay = true;
    }

    if (this.gifSupport) {
      this.updateStyles({
        '--emoji-category-size': constants.V2_5_EMOJI_CATEGORY_SIZE_PX,
        '--emoji-group-button-size': constants.V2_5_EMOJI_GROUP_SIZE_PX,
        '--emoji-picker-side-padding':
            constants.V2_5_EMOJI_PICKER_SIDE_PADDING_PX,
        '--emoji-picker-search-side-padding':
            constants.V2_5_EMOJI_PICKER_SEARCH_SIDE_PADDING_PX,
        '--emoji-spacing': constants.V2_5_EMOJI_SPACING_PX,
        '--emoji-group-spacing': constants.V2_5_EMOJI_GROUP_SPACING_PX,
        '--visual-content-width': constants.V2_5_VISUAL_CONTENT_WIDTH_PX,
      });
    }

    // Update UI and relevant features based on the initial data.
    this.updateCategoryData(
        // If we don't have 1 data URL, a crash probably isn't a bad idea
        initialData, dataUrls[0]!.category, dataUrls[0]!.categoryLastPartition,
        false);

    // Show the UI after the initial data is rendered.
    afterNextRender(this, () => {
      this.apiProxy.showUi();
    });

    // Filter data, remove the first url as it is
    // already added and shown.
    const remainingData = dataUrls.slice(1);

    let prevFetchPromise: Promise<EmojiGroupData> = Promise.resolve([]);
    let prevRenderPromise = Promise.resolve();

    // Create a chain of promises for fetching and rendering data of
    // different categories in the correct order.
    remainingData.forEach(
        (dataUrl, index) => {
          // Fetch the url only after the previous url is fetched.
          prevFetchPromise =
              prevFetchPromise.then(() => this.fetchOrderingData(dataUrl.url));

          // Update category data after the data is fetched and the previous
          // category data update/rendering completed successfully.
          prevRenderPromise = Promise
                                  .all(
                                      [prevRenderPromise, prevFetchPromise],
                                      )
                                  // Hacky cast below, but should be safe
                                  .then((values) => values[1])
                                  .then(
                                      (data) => this.updateCategoryData(
                                          data,
                                          dataUrl.category,
                                          dataUrl.categoryLastPartition,
                                          index === remainingData.length - 1,
                                          ),
                                  );
        },
    );

    if (this.gifSupport) {
      await this.fetchAndProcessGifData(prevFetchPromise, prevRenderPromise);
    }
  }

  private fetchAndProcessGifData(
      prevFetchPromise: Promise<EmojiGroupData> = Promise.resolve([]),
      prevRenderPromise = Promise.resolve()): Promise<void> {
    this.validateRecentlyUsedGifs();

    // Set Recently Used and Trending in the GIF tabs first before fetching
    // Tenor API data
    const trendingGifData = {name: constants.TRENDING};
    const initialCategoryTabs = {
      ...CATEGORY_TABS,
      gif: this.setGifGroupsPagination([trendingGifData]),
    };
    this.allCategoryTabs = gifCategoryTabs(initialCategoryTabs);

    // Fetch Tenor API category groups
    const categoriesFetchPromise =
        prevFetchPromise.then(() => this.apiProxy.getCategories());

    const categoriesRenderPromise =
        Promise.all([prevRenderPromise, categoriesFetchPromise])
            .then((values) => {
              const {gifCategories} = values[1];
              const categoryTabs = {
                ...CATEGORY_TABS,
                gif: this.setGifGroupsPagination(
                    [trendingGifData, ...gifCategories]),
              };

              gifCategories.map(
                  category => this.nextGifPos[category.name] = '');
              this.allCategoryTabs = gifCategoryTabs(categoryTabs);

              // If user is on GIF category, update emojiGroupTabs to
              // re-render emoji picker and display newly fetched GIF tabs
              if (this.category === CategoryEnum.GIF) {
                const gifTabs = this.allCategoryTabs.filter(
                    tab => tab.category === CategoryEnum.GIF);
                this.set('emojiGroupTabs', gifTabs);
                this.updateActiveGroup();
              }
            });
    const featuredGifFetchPromise =
        categoriesFetchPromise.then(() => this.apiProxy.getFeaturedGifs());

    return Promise.all([categoriesRenderPromise, featuredGifFetchPromise])
        .then((values) => {
          const {status, featuredGifs} = values[1];
          this.status = status;
          const trendingGifsElement: EmojiVariants[] =
              this.apiProxy.convertTenorGifsToEmoji(featuredGifs);

          const trendingGifs = [{
            group: constants.TRENDING,
            category: CategoryEnum.GIF,
            emoji: trendingGifsElement,
          }];
          this.nextGifPos[constants.TRENDING] = featuredGifs.next;

          this.updateCategoryData(trendingGifs, CategoryEnum.GIF);
          this.activeInfiniteGroupId =
              this.allCategoryTabs.find(tab => tab.name === constants.TRENDING)
                  ?.groupId as string;
        });
  }

  onClickTryAgain() {
    // The network error illustration only displays in GIF panel under offline
    // mode; in this case, after reloading data, we should switch back to GIF
    // panel (if it is successful).
    this.fetchAndProcessGifData().then(() => {
      this.onCategoryButtonClick(CategoryEnum.GIF);
    });
  }

  private canScrollToGroup(category: CategoryEnum, groupId: string): boolean {
    if (!this.isInfiniteCategory(category)) {
      return true;
    }

    // There will always be a trending GIF group (initialised at beginning),
    // hence it is safe to cast as SubcategoryData
    const trendingGifGroup =
        this.allCategoryTabs.find(
            tab =>
                (tab.category === CategoryEnum.GIF &&
                 tab.name === constants.TRENDING)) as SubcategoryData;
    const historyGifGroup = this.categoriesGroupElements.find(
        group => group.category === CategoryEnum.GIF && group.isHistory);
    return [trendingGifGroup.groupId, historyGifGroup?.groupId].includes(
        groupId);
  }

  private isInfiniteCategory(category: CategoryEnum) {
    return category === CategoryEnum.GIF;
  }

  private setActiveFeatures(featureList: Feature[]) {
    this.gifSupport = featureList.includes(Feature.EMOJI_PICKER_GIF_SUPPORT);
    this.useMojoSearch = featureList.includes(Feature.EMOJI_PICKER_MOJO_SEARCH);
    this.sealSupport = featureList.includes(Feature.EMOJI_PICKER_SEAL_SUPPORT);
    this.variantGroupingSupport =
        featureList.includes(Feature.EMOJI_PICKER_VARIANT_GROUPING_SUPPORT);

    this.updateEmojiPreferencesStore();
  }

  private fetchOrderingData(url: string): Promise<EmojiGroupData> {
    return new Promise((resolve) => {
      const xhr = new XMLHttpRequest();
      xhr.onloadend = () => resolve(JSON.parse(xhr.responseText));
      xhr.open('GET', url);
      xhr.send();
    });
  }

  /**
   * Processes a new category data and updates any needed variables and UIs
   * accordingly.
   *
   * @param data The category data to be processes.
   *    Note: category field will be added to the each EmojiGroup in data.
   * @param category Category of the data.
   * @param categoryLastPartition True if no future data updates are expected
   *     for the given category.
   * @param lastPartition True if no future data updates are expected.
   */
  private updateCategoryData(
      data: EmojiGroupData, category: CategoryEnum,
      categoryLastPartition = false, lastPartition = false) {
    // TODO(b/233270589): Add category to the underlying data.
    // Add category field to the data.
    data.forEach((emojiGroup) => {
      emojiGroup.category = category;
    });

    // Create recently used emoji group for the category as its first
    // group element.
    const startIndexes = this.gifSupport ?
        TABS_CATEGORY_START_INDEX_GIF_SUPPORT :
        TABS_CATEGORY_START_INDEX;
    const startIndex = startIndexes.get(category);
    if (startIndex === this.categoriesGroupElements.length) {
      const historyGroupElement = this.createEmojiGroupElement(
          this.getHistoryEmojis(category), {}, true, startIndex);
      this.push('categoriesGroupElements', historyGroupElement);
    }

    // Convert the emoji group data to elements.
    const baseIndex = this.categoriesGroupElements.length;
    const categoriesGroupElements: EmojiGroupElement[] = [];

    data.filter(item => !item.searchOnly).forEach((emojiGroup, index) => {
      if (emojiGroup.category === CategoryEnum.GIF &&
          emojiGroup.emoji.length === 0) {
        // EmojiGroup.emoji will be empty if and only if it is a gif category
        // and there's an error when trying to fetch gifs.
        return;
      }

      if (this.variantGroupingSupport && category === CategoryEnum.EMOJI) {
        emojiGroup.emoji.forEach((emoji) => {
          this.fillEmojiVariantAttributes(emoji);
        });

        this.categoryHistoryUpdated(CategoryEnum.EMOJI);
      }

      const tabIndex = baseIndex + index;
      const tabCategory = this.allCategoryTabs[tabIndex]?.category;

      categoriesGroupElements.push(
          this.createEmojiGroupElement(
              emojiGroup.emoji, this.getEmojiGroupPreference(category), false,
              tabIndex),
      );

      // TODO(b/233271528): Remove assert after removing metadata.
      // Ensure category of emoji groups match tab entries.
      console.assert(
          tabCategory === category,
          `Tab category at index ${tabIndex} is ${tabCategory} ` +
              `but corresponding group category in data is ${category}.`);
    });

    // Update emoji data for other features such as search.
    if (category !== CategoryEnum.GIF) {
      this.push('categoriesData', ...data);
    }
    // Update group elements for the emoji picker.
    this.push('categoriesGroupElements', ...categoriesGroupElements);

    if (categoryLastPartition) {
      this.dispatchEvent(events.createCustomEvent(
          events.CATEGORY_DATA_LOADED, {'category': category}));
    }

    if (lastPartition) {
      // If all data is fetched, trigger search index.
      this.searchLazyIndexing = false;

      // TODO(b/233271528): Remove the following after removing metadata.
      const numEmojiGroups = this.categoriesGroupElements.length;
      const dataMatchSubcategoryTabs =
          numEmojiGroups === this.allCategoryTabs.length;

      // Ensure hard-coded tabs match the loaded data.
      if (!this.gifSupport) {
        console.assert(
            dataMatchSubcategoryTabs,
            `The Number of tabs "${
                this.allCategoryTabs.length}" does not match ` +
                ` the number of loaded groups "${numEmojiGroups}".`,
        );
      }

      afterNextRender(
          this,
          async () => {
            switch ((await this.apiProxy.getInitialCategory()).category) {
              // by default, do nothing.
              default:
              case Category.kEmojis:
                break;
              case Category.kSymbols:
                await this.onCategoryButtonClick(CategoryEnum.SYMBOL);
                break;
              case Category.kEmoticons:
                await this.onCategoryButtonClick(CategoryEnum.EMOTICON);
                break;
              case Category.kGifs:
                await this.onCategoryButtonClick(CategoryEnum.GIF);
                break;
            }

            const initialQuery = (await this.apiProxy.getInitialQuery()).query;
            if (initialQuery !== '') {
              this.$['search-container'].setSearchQuery(initialQuery);
            }

            this.apiProxy.onUiFullyLoaded();
            this.dispatchEvent(
                events.createCustomEvent(events.EMOJI_PICKER_READY, {}));
          },
      );
    }
  }

  /**
   * Fills any gaps in the tone and gender information for variants of the
   * emoji; the build script omits this information in some cases to reduce
   * build size. Variant and grouping information is also copied to the
   * corresponding history emoji, if it exists, because existing store data
   * may not have the information.
   */
  private fillEmojiVariantAttributes(emoji: EmojiVariants) {
    const {base, alternates, groupedTone, groupedGender} = emoji;

    if (!base.name || !alternates || !(groupedTone || groupedGender)) {
      return;
    }

    alternates.forEach((variant) => {
      if (groupedTone) {
        variant.tone ??= Tone.DEFAULT;
      }

      if (groupedGender) {
        variant.gender ??= Gender.DEFAULT;
      }
    });

    this.categoriesHistory[CategoryEnum.EMOJI]?.fillEmojiVariantAttributes(
        base.name, alternates, groupedTone, groupedGender);
  }

  private onSearchChanged(newValue: string) {
    this.$['list-container'].style.display = newValue ? 'none' : '';
  }

  private onBarTransitionStart() {
    this.highlightBarMoving = true;
  }

  private onBarTransitionEnd() {
    this.highlightBarMoving = false;
  }

  private onEmojiTextButtonClick(ev: events.EmojiTextButtonClickEvent) {
    const category = ev.detail.category;
    this.insertText(category, ev.detail);
  }

  private onEmojiImgButtonClick(ev: events.EmojiImgButtonClickEvent) {
    const category = ev.detail.category;
    this.insertVisualContent(category, ev.detail);
  }

  private async insertText(category: CategoryEnum, item: events.TextItem) {
    const {text, isVariant} = item;
    this.$.message.textContent = text + ' inserted.';

    this.insertHistoryTextItem(category, item);

    const searchLength = this.$['search-container']
                             .shadowRoot!.querySelector('cr-search-field')
                             ?.getSearchInput()
                             ?.value?.length ??
        0;

    // TODO(b/217276960): change to a more generic name
    this.apiProxy.insertEmoji(text, isVariant, searchLength);
  }

  private insertVisualContent(category: CategoryEnum, item: events.VisualItem) {
    this.apiProxy.insertGif(item.visualContent.url.full);
    this.insertHistoryVisualContentItem(category, item);
  }

  isGifInErrorState(status: Status): boolean {
    return this.gifSupport && status !== Status.kHttpOk;
  }

  private clearRecentEmoji(event: events.EmojiClearRecentClickEvent) {
    const category = event.detail.category;
    const item = event.detail.item;
    this.clearHistoryData(category, item);
    afterNextRender(this, () => {
      this.updateActiveGroup();
      this.updateHistoryTabDisabledProperty();
    });
  }

  private async setGifGroupElements(activeGroupId: string) {
    // Check if GIFs have already been previously fetched and cached
    let gifGroupElements = this.categoriesGroupElements.find(
        group => group.groupId === activeGroupId);
    const isCached = !!gifGroupElements;

    // Call API only if the GIF elements for this search query has not been
    // cached
    if (!gifGroupElements) {
      // searchQuery will never be undefined
      const searchQuery =
          this.allCategoryTabs
              .find(element => element.groupId === activeGroupId)
              ?.name as string;
      const {searchGifs} = await this.apiProxy.searchGifs(searchQuery);
      const gifElements = this.apiProxy.convertTenorGifsToEmoji(searchGifs);
      this.nextGifPos[searchQuery] = searchGifs.next;

      const activeCategoryIndex =
          this.allCategoryTabs.findIndex(tab => tab.groupId === activeGroupId);

      gifGroupElements = this.createEmojiGroupElement(
          gifElements, {}, false, activeCategoryIndex);
    }

    this.setActiveInfiniteGroup(activeGroupId);
    if (!isCached) {
      this.categoriesGroupElements =
          [...this.categoriesGroupElements, gifGroupElements];
    }
  }

  private setActiveInfiniteGroup(activeGroupId: string) {
    this.updateActiveGroup(activeGroupId);
    this.activeInfiniteGroupId = activeGroupId;
  }

  private async selectGroup(newGroup: string) {
    await this.ensureFetchAndProcessDataFinished();

    if (this.category === CategoryEnum.GIF) {
      await this.setGifGroupElements(newGroup);
    }

    // focus and scroll to selected group's first emoji.
    const group =
        this.shadowRoot?.querySelector(`div[data-group="${newGroup}"]`);

    if (group) {
      const target = (group.querySelector('.group')?.shadowRoot?.querySelector(
                         '#fake-focus-target')) as HTMLElement |
          null;
      target?.focus();
      group.scrollIntoView();
    }
  }

  private onEmojiScroll() {
    // The scroll event is fired very frequently while scrolling.
    // Thus we wrap it with `requestAnimationFrame`
    requestAnimationFrame(() => {
      this.updateActiveCategory();
      this.updateActiveGroup();
      // Using ! here as this.status will always exist when GIF support is on.
      if (this.category === CategoryEnum.GIF &&
          !this.isGifInErrorState(this.status!)) {
        this.checkScrollPosition();
      }
    });
  }

  private onRightChevronClick() {
    if (!this.textSubcategoryBarEnabled) {
      // ! safe due to &&
      this.$.tabs.scrollLeft = constants.EMOJI_PICKER_TOTAL_EMOJI_WIDTH *
          constants.EMOJI_NUM_TABS_IN_FIRST_PAGE;
      this.scrollToGroup(
          EMOJI_GROUP_TABS[constants.GROUP_PER_ROW - 1]?.groupId);
      this.$.bar.style.left = constants.EMOJI_PICKER_TOTAL_EMOJI_WIDTH_PX;
    } else {
      const maxPagination =
          this.getPaginationArray(this.emojiGroupTabs).pop() ?? 0;
      this.pagination = Math.min(this.pagination + 1, maxPagination);
      this.updateCurrentGroupTabs();
    }
  }

  private onRightChevronKeyDown(event: KeyboardEvent) {
    // Moves focus to the first button under the group of current category if
    // user tries to move to the next element from right chevron button in a11y
    // mode.
    if (event.code === 'Tab' && !event.shiftKey) {
      const currentGroups = this.shadowRoot!
        .querySelectorAll<EmojiGroupComponent>(
          `emoji-group[category='${this.category}'`);

      // The first group might be a history group. If the user has no history
      // item, we should continue to check the second group.
      for (const group of currentGroups) {
        const button = group.firstEmojiButton();
        if (button) {
          button.focus();
          event.preventDefault();
          return;
        }
      }
    }

    // Announcement for a11y.
    getAnnouncerInstance().announce('New sections available');
  }

  private onLeftChevronClick() {
    this.pagination = Math.max(this.pagination - 1, 1);
    this.updateCurrentGroupTabs();

    // Announcement for a11y.
    getAnnouncerInstance().announce('New sections available');
  }

  private updateCurrentGroupTabs() {
    const nextTab =
        this.emojiGroupTabs.find((tab) => tab.pagination === this.pagination);
    if (this.category === CategoryEnum.GIF && nextTab) {
      this.setGifGroupElements(nextTab.groupId);
    }
    this.scrollToGroup(nextTab?.groupId);
  }

  scrollToGroup(newGroup?: string) {
    // TODO(crbug/1152237): This should use behaviour:'smooth', but when you do
    // that it doesn't scroll.
    if (newGroup) {
      this.shadowRoot!.querySelector(`div[data-group="${newGroup}"]`)
          ?.scrollIntoView();
    }
  }

  private onGroupsScroll() {
    this.updateChevrons();

    // This stops the GIF group tabs bar from bouncing back
    // when clicking on left/right chevron.
    if (this.category === CategoryEnum.GIF) {
      return;
    }

    requestAnimationFrame(() => this.updateActiveGroup());
  }

  private updateChevrons() {
    const leftChevron = this.$['left-chevron'];
    const rightChevron = this.$['right-chevron'];
    // bail early if required elements don't exist
    if (!leftChevron || !rightChevron) {
      return;
    }
    if (!this.textSubcategoryBarEnabled) {
      leftChevron.style.display = 'none';
      rightChevron.style.display = 'none';
    } else {
      leftChevron.style.display = this.pagination >= 2 ? 'flex' : 'none';
      rightChevron.style.display = this.pagination <
              (this.getPaginationArray(this.emojiGroupTabs).pop() ?? 0) ?
          'flex' :
          'none';
    }
  }

  /**
   * @returns the id of the emoji or emoticon group currently in view.
   * This does not apply to GIFs, since each infinite set of GIF elements for
   * the GIF categories are displayed on a separate page, and cannot be
   * accessible from other categories via scrolling.
   */
  private getActiveGroupIdFromScrollPosition(): string {
    // get bounding rect of scrollable emoji region.
    const thisRect = this.$.groups.getBoundingClientRect();

    return this.getActiveGroupAndId(thisRect).id;
  }

  getActiveGroupAndId(thisRect: DOMRect):
      {group: HTMLElement|undefined, id: string} {
    const groupElements = Array.from(
        this.$['groups']?.querySelectorAll<HTMLElement>('[data-group]') ?? []);

    // activate the first group which is visible for at least 20 pixels,
    // i.e. whose bottom edge is at least 20px below the top edge of the
    // scrollable region.
    const activeGroup = groupElements.find(
        el => el.getBoundingClientRect().bottom - thisRect.top >= 20);

    let activeGroupId;
    if (activeGroup === undefined) {
      if (this.status && this.isGifInErrorState(this.status)) {
        // If there's an error Trending gifs will be empty, so activeGroup
        // cannot be found from scroll position, have to set it manually.
        activeGroupId = constants.TRENDING_GROUP_ID;
      } else {
        activeGroupId = 'emoji-history';
      }
    } else {
      activeGroupId = activeGroup.dataset['group'] ?? '';
    }
    return {group: activeGroup, id: activeGroupId};
  }

  private async checkScrollPosition(): Promise<void> {
    if (this.activeInfiniteGroupId === null) {
      return;
    }

    // get bounding rect of scrollable emoji region.
    const thisRect = this.$.groups.getBoundingClientRect();

    const activeGroupInfo = this.getActiveGroupAndId(thisRect);
    if (!activeGroupInfo.group) {
      return;
    }

    // Don't append new GIFs if the initial set is still rendering.
    if (activeGroupInfo.group.getBoundingClientRect().height <=
        thisRect.height) {
      return;
    }

    // If there's less than 300px until the end of the GIF category, load more
    // GIFs.
    if (activeGroupInfo.group.getBoundingClientRect().bottom -
            thisRect.bottom <=
        300) {
      // Using ! here as you can only scroll on a GIF category after the first
      // set of fetched GIFs have been pushed to this.categoriesGroupElements,
      // i.e. There will always be an element group that matches with the
      // current activeInfiniteGroupId.
      const searchQuery =
          this.categoriesGroupElements
              .find(
                  group => group.groupId === this.activeInfiniteGroupId)!.name;
      // No need to append to history group.
      if (searchQuery === constants.RECENTLY_USED) {
        return;
      }

      let gifElements;
      if (searchQuery === constants.TRENDING) {
        const {featuredGifs} =
            await this.apiProxy.getFeaturedGifs(this.nextGifPos[searchQuery]);
        gifElements = featuredGifs;
      } else {
        const {searchGifs} = await this.apiProxy.searchGifs(
            searchQuery, this.nextGifPos[searchQuery]);
        gifElements = searchGifs;
      }

      this.nextGifPos[searchQuery] = gifElements.next;
      const gifs = this.apiProxy.convertTenorGifsToEmoji(gifElements);
      this.appendGifElements(searchQuery, gifs);
    }
  }

  private appendGifElements(subcategory: string, gifs: EmojiVariants[]) {
    const categoryIndex =
        this.categoriesGroupElements.findIndex(tab => tab.name === subcategory);
    if (categoryIndex === -1) {
      return;
    }
    this.push(['categoriesGroupElements', categoryIndex, 'emoji'], ...gifs);
  }

  /*
   * Active group is updated with scroll position for emoji types with finite
   * elements (emojis, emoticons, symbols). However, active groups for emoji
   * types with infinite will be passed through a groupId because they cannot be
   * determined with scroll position.
   */
  private updateActiveGroup(groupId?: string) {
    let activeGroupId = groupId;
    if (groupId == null) {
      activeGroupId = this.getActiveGroupIdFromScrollPosition();
    }

    if (this.category === CategoryEnum.GIF) {
      this.set('activeInfiniteGroupId', activeGroupId);
    }

    this.set(
        'pagination', this.getPaginationFromGroupId(activeGroupId as string));
    this.updateChevrons();
    const bar = this.$.bar;

    let index = 0;
    // set active to true for selected group and false for others.
    this.emojiGroupTabs.forEach((g, i) => {
      const isActive = g.groupId === activeGroupId;
      if (isActive) {
        index = i;
      }
      this.set(['emojiGroupTabs', i, 'active'], isActive);
    });

    // Ensure that the history tab is not set as active if it is empty.
    if (index === 0 && this.isCategoryHistoryEmpty(this.category)) {
      this.set(['emojiGroupTabs', 0, 'active'], false);
      this.set(['emojiGroupTabs', 1, 'active'], true);
      index = 1;
    }

    // Once tab scroll is updated, update the position of the highlight bar.
    if (!this.highlightBarMoving) {
      // Update the scroll position of the emoji groups so that active group is
      // visible.
      if (!this.textSubcategoryBarEnabled) {

        // The value here means the width of an emoji tab button + extra right
        // side spacing. It's different between the versions before GIF support
        // and after.
        const totalWidth = this.gifSupport ?
          constants.V2_5_EMOJI_PICKER_TOTAL_EMOJI_WIDTH :
          constants.EMOJI_PICKER_TOTAL_EMOJI_WIDTH;

        bar.style.width = constants.EMOJI_HIGHLIGHTER_WIDTH_PX;
        bar.style.left = `${index * totalWidth}px`;
      } else {
        // Cast below should be safe as worst case array is empty.
        const subcategoryTabs =
            Array.from(this.$.tabs.querySelectorAll<HTMLElement>('.tab'));

        // for text group button, the highlight bar only spans its inner width,
        // which excludes both padding and margin.
        if (index < subcategoryTabs.length) {
          const padding = this.gifSupport ?
              constants.V2_5_EMOJI_PICKER_SIDE_PADDING :
              constants.EMOJI_PICKER_SIDE_PADDING;
          const barInlineGap =
              constants.TAB_BUTTON_MARGIN + constants.TEXT_GROUP_BUTTON_PADDING;
          const currentTab = subcategoryTabs[index];
          bar.style.left = `${
              (currentTab?.offsetLeft ?? 0) - padding -
              this.calculateTabScrollLeftPosition(this.pagination)}px`;
          bar.style.width = `${
              (subcategoryTabs[index]?.clientWidth ?? 0) - barInlineGap * 2}px`;
        } else {
          bar.style.left = '0px';
          bar.style.width = '0px';
        }
      }
    }
  }

  /**
   * Update active category by using vertical scroll position.
   */
  private updateActiveCategory() {
    const activeGroupId = this.getActiveGroupIdFromScrollPosition();

    const currentCategory =
        this.allCategoryTabs.find((tab) => tab.groupId === activeGroupId)
            ?.category;
    this.set('category', currentCategory);
  }

  private preventV2TabScrolling() {
    this.$.tabs.scrollLeft =
        this.calculateTabScrollLeftPosition(this.pagination);
  }

  private hideDialogs() {
    this.hideEmojiVariants();

    for (const category of Object.values(CategoryEnum)) {
      if (!this.isCategoryHistoryEmpty(category)) {
        const historyButton =
            this.shadowRoot!.querySelector<EmojiGroupComponent>(
                `emoji-group.history[category=${category}]`);
        if (historyButton) {
          historyButton.showClearRecents = false;
        }
      }
    }
  }

  private hideEmojiVariants() {
    if (this.activeVariant) {
      this.activeVariant.hideEmojiVariants();
      this.activeVariant = null;
    }
  }

  /**
   * Disables the history tab when there is no usage history for the
   * selected category and enables it otherwise.
   */
  private updateHistoryTabDisabledProperty() {
    this.set(
        ['emojiGroupTabs', 0, 'disabled'],
        this.isCategoryHistoryEmpty(this.category),
    );
  }

  /**
   * Gets recently used emojis for a category. It gets the history items
   * and convert them to emojis.
   */
  private getHistoryEmojis(category: CategoryEnum): EmojiVariants[] {
    if (this.incognito) {
      return [];
    }

    return this.categoriesHistory[category]?.getHistory().map(
               emoji => ({
                 base: {
                   string: emoji.base.string,
                   name: emoji.base.name,
                   visualContent: emoji.base.visualContent,
                   keywords: [],
                   tone: emoji.base.tone,
                   gender: emoji.base.gender,
                 },
                 alternates: emoji.alternates?.map(
                                 (alternate: Emoji):
                                     Emoji => {
                                       return {
                                         string: alternate.string,
                                         name: alternate.name,
                                         keywords:
                                             [...(alternate.keywords ?? [])],
                                         tone: alternate.tone,
                                         gender: alternate.gender,
                                       };
                                     }) ??
                     [],
                 groupedTone: emoji.groupedTone,
                 groupedGender: emoji.groupedGender,
               })) ??
        [];
  }

  /**
   * Handles the event where history or preferences are modified for a
   * category.
   *
   */
  private categoryHistoryUpdated(
      category: CategoryEnum, historyUpdated = true,
      _preferenceUpdated = true) {
    // History item is assumed to be the first item of each category.
    const historyIndexes = this.gifSupport ?
        TABS_CATEGORY_START_INDEX_GIF_SUPPORT :
        TABS_CATEGORY_START_INDEX;
    const historyIndex = historyIndexes.get(category);

    // If history group is already added, then update it.
    if (historyUpdated && (historyIndex !== undefined) &&
        historyIndex < this.categoriesGroupElements.length) {
      this.set(
          ['categoriesGroupElements', historyIndex, 'emoji'],
          this.getHistoryEmojis(category));
    }

    // Note: preference update is not handled because it is an expensive
    // operation and adds no value to the current version.
    // If needed in the future, its addition requires iterating over all
    // categoriesGroupElement of the category and setting their preferences
    // here.
  }

  /**
   * Updates incognito state and variables that needs to be updated by the
   * change of incognito state.
   *
   */
  async updateIncognitoState(incognito: boolean) {
    this.incognito = incognito;
    this.updateEmojiPreferencesStore();

    // Load the history item for each category.
    // Initialise all objects before async for extra safety.
    for (const category of Object.values(CategoryEnum)) {
      this.categoriesHistory[category] =
          incognito ? null : new RecentlyUsedStore(category);
    }
    for (const category of Object.values(CategoryEnum)) {
      await this.categoriesHistory[category]?.mergeWithPrefsHistory();
      this.categoryHistoryUpdated(category);
    }
  }

  /**
   * Updates the emoji preferences store, global tone, and global gender.
   */
  updateEmojiPreferencesStore() {
    this.emojiPreferences = this.incognito || !this.variantGroupingSupport ?
        null :
        new EmojiPreferencesStore();
    this.globalTone = this.emojiPreferences?.getTone() ?? null;
    this.globalGender = this.emojiPreferences?.getGender() ?? null;
  }

  /**
   * Inserts a new item to the history of a category. It will do nothing during
   * incognito state.
   *
   */
  private insertHistoryTextItem(category: CategoryEnum, item: events.TextItem) {
    if (this.incognito) {
      return;
    }

    const {
      text,
      baseEmoji,
      alternates,
      name,
      tone,
      gender,
      groupedTone,
      groupedGender,
    } = item;

    this.categoriesHistory[category]?.bumpItem(category, {
      base: {string: text, name, tone, gender},
      alternates,
      groupedTone,
      groupedGender,
    });

    let preferenceUpdated = false;

    if (!this.variantGroupingSupport || !(groupedTone || groupedGender)) {
      preferenceUpdated =
          !!this.categoriesHistory[category]?.savePreferredVariant(
              text, baseEmoji);
    }

    this.categoryHistoryUpdated(category, true, preferenceUpdated);

    if (!this.variantGroupingSupport) {
      return;
    }

    if (tone !== undefined) {
      this.emojiPreferences?.setTone(tone);
    }

    if (gender !== undefined) {
      this.emojiPreferences?.setGender(gender);
    }
  }

  /**
   * Inserts a new item to the history of a visual content category. It will do
   * nothing during incognito state.
   */
  private insertHistoryVisualContentItem(
      category: CategoryEnum, item: events.VisualItem) {
    if (this.incognito) {
      return;
    }

    const {name, visualContent} = item;

    this.categoriesHistory[category]?.bumpItem(
        category,
        {base: {visualContent: visualContent, name: name}, alternates: []});

    this.categoryHistoryUpdated(category, true, undefined);
  }

  /**
   * Clears history item(s) for a category.
   */
  private clearHistoryData(category: CategoryEnum, item?: EmojiVariants) {
    if (this.incognito) {
      return;
    }

    if (item === undefined) {
      this.categoriesHistory[category]?.clearRecents();
    } else {
      this.categoriesHistory[category]?.clearItem(category, item);
    }

    this.categoryHistoryUpdated(category, true, false);
  }

  /**
   * Check if the history items of a category is empty.
   *
   * @returns {boolean} True for empty history.
   */
  private isCategoryHistoryEmpty(category: CategoryEnum) {
    return this.incognito || this.categoriesHistory[category]?.isHistoryEmpty();
  }

  /**
   * @returns True if the emoji should use the global variant preference, or
   * false if it should revert to the individual preference.
   */
  private shouldUseGroupedPreference(isHistory: boolean): boolean {
    return this.variantGroupingSupport && !isHistory;
  }

  /**
   * Gets HTML classes for an emoji group element.
   *
   * The emojis need to be passed in directly so Polymer registers changes
   * e.g. when clearing history emojis.
   * The currEmojiGroup is passed in for additional attribute info.
   *
   * @returns {string} HTML element class attribute.
   */
  private getEmojiGroupClassNames(
      emojis: EmojiVariants[], currEmojiGroup: EmojiGroupElement,
      activeCategory: CategoryEnum, activeInfiniteGroupId: string) {
    const baseClassNames = currEmojiGroup.isHistory ? 'group history' : 'group';

    // Make emoji hidden if it is empty.
    // Note: Filtering empty groups in dom-repeat is expensive due to
    // re-rendering, so making it hidden is more efficient.
    if (!emojis || emojis.length === 0) {
      return baseClassNames + ' hidden';
    }

    if (!this.gifSupport) {
      return baseClassNames;
    }

    // If the active group can be reached via scrolling (i.e. emojis, emoticons,
    // symbols, recently used GIFs, trending GIFs), whereas the current group
    // being rendered cannot, the current group should be hidden, and vice
    // versa. i.e. When on an emoji group, you can scroll down to other emoticon
    // or symbol groups, or recently used or trending GIFs, but cannot scroll
    // past trending GIFs to view #tag GIF groups.
    const canScrollToActiveGroup =
        this.canScrollToGroup(activeCategory, activeInfiniteGroupId);
    const canScrollToCurrGroup =
        this.canScrollToGroup(currEmojiGroup.category, currEmojiGroup.groupId);
    const canScrollToCurrGroupFromActive =
        (canScrollToCurrGroup && canScrollToActiveGroup);

    // For GIF category groups (not including history or recently used), only
    // the currently active group should be displayed and all other groups
    // should be hidden.
    const currGroupIsActiveInfiniteGroup =
        currEmojiGroup.groupId === activeInfiniteGroupId;

    if (!canScrollToCurrGroupFromActive && !currGroupIsActiveInfiniteGroup) {
      return baseClassNames + ' hidden';
    }
    return baseClassNames;
  }

  /**
   * Create an instance of emoji group element.
   */
  private createEmojiGroupElement(
      emoji: EmojiVariants[], preferences: PreferenceMapping,
      isHistory: boolean, subcategoryIndex: number): EmojiGroupElement {
    const baseDetails = {
      'emoji': emoji,
      'preferences': preferences,
      'isHistory': isHistory,
    };

    return (
        Object.assign({}, baseDetails, this.allCategoryTabs[subcategoryIndex]));
  }

  /**
   * Gets preferences for an emoji group.
   *
   */
  private getEmojiGroupPreference(category: CategoryEnum): PreferenceMapping {
    return this.categoriesHistory[category]?.getPreferenceMapping() ?? {};
  }

  private onShowEmojiVariants(ev: events.EmojiVariantsShownEvent) {
    // Hide the currently shown emoji variants if the new one belongs
    // to a different emoji group.
    if (this.activeVariant && ev.detail.owner !== this.activeVariant) {
      this.hideEmojiVariants();
    }

    this.activeVariant = ev.detail.owner as EmojiGroupComponent;

    // Updates the UI if a variant is shown.
    if (ev.detail.variants) {
      this.$.message.textContent = ev.detail.baseEmoji + ' variants shown.';
      this.positionEmojiVariants(ev.detail.variants);
    }
  }

  private positionEmojiVariants(variants: HTMLElement) {
    // TODO(crbug.com/1174311): currently positions horizontally within page.
    // ideal UI would be overflowing the bounds of the page.
    // also need to account for vertical positioning.

    // compute width required for the variant popup as: SIZE * columns + 10.
    // SIZE is emoji width in pixels. number of columns is determined by width
    // of variantRows, then one column each for the base emoji and skin tone
    // indicators if present. 10 pixels are added for padding and the shadow.

    // Reset any existing left margin before calculating a new position.
    variants.style.marginLeft = '0';

    // get size of emoji picker
    const pickerRect = this.getBoundingClientRect();

    // determine how much overflows the right edge of the window.
    const rect = variants.getBoundingClientRect();
    const overflowWidth = rect.x + rect.width - pickerRect.width;
    // shift left by overflowWidth rounded up to next multiple of EMOJI_SIZE.
    const shift = constants.EMOJI_ICON_SIZE *
        Math.ceil(overflowWidth / constants.EMOJI_ICON_SIZE);
    // negative value means we are already within bounds, so no shift needed.
    variants.style.marginLeft = `-${Math.max(shift, 0)}px`;
    // Now, examine vertical scrolling and scroll if needed. Not quire sure why
    // we need listcontainer.offsetTop, but it makes things work.
    const groups = this.$.groups;
    const scrollTop = groups.scrollTop;
    const variantTop = variants.offsetTop;
    const variantBottom = variantTop + variants.offsetHeight;
    const listTop = this.$['list-container'].offsetTop;
    if (variantBottom > scrollTop + (groups.offsetHeight) + listTop) {
      groups.scrollTo({
        top: variantBottom - (groups.offsetHeight) - listTop,
        left: 0,
        behavior: 'smooth',
      });
    }
  }

  /**
   * Triggers when category property changes
   */
  private onCategoryChanged(newCategoryName: string) {
    const categoryTabs =
        this.allCategoryTabs.filter(tab => tab.category === newCategoryName);
    this.set('emojiGroupTabs', categoryTabs);
    this.updateActiveGroup();
    this.updateHistoryTabDisabledProperty();
    this.$.tabs.scrollLeft =
        this.calculateTabScrollLeftPosition(this.pagination);
  }

  private async onCategoryButtonClick(newCategory: CategoryEnum) {
    await this.ensureFetchAndProcessDataFinished();

    this.set('category', newCategory);
    this.set('pagination', 1);

    if (newCategory === CategoryEnum.GIF) {
      this.set('activeInfiniteGroupId', this.emojiGroupTabs[0]?.groupId);
    }

    if (this.$['search-container'].searchNotEmpty()) {
      this.$['search-container'].setSearchQuery('');
      afterNextRender(this, () => {
        this.scrollToGroup(this.emojiGroupTabs[0]?.groupId);
      });
    } else {
      this.scrollToGroup(this.emojiGroupTabs[0]?.groupId);
    }
  }

  /**
   * Trigger when pagination changes
   */
  private onPaginationChanged(newPage: number) {
    // Left chevron has the same margin as the text subcategory button.
    this.$.tabs.scrollLeft = this.calculateTabScrollLeftPosition(newPage);
  }

  /**
   * Returns true if the subcategory bar requires text group buttons.
   */
  private isTextSubcategoryBarEnabled(category: string) {
    // Categories that require its subcategory bar to be labelled by text.
    const textCategories = ['symbol', 'emoticon', 'gif'];
    return textCategories.includes(category);
  }

  /**
   * Returns the array of page numbers which starts at 1 and finishes at the
   * last pagination.
   */
  private getPaginationArray(tabs: SubcategoryData[]) {
    // cast safe here since it is the result of filter.
    const paginations =
        tabs.map(tab => tab.pagination).filter(num => num) as number[];
    const lastPagination = Math.max(...paginations);
    return Array.from(Array(lastPagination), (_, idx) => idx + 1);
  }

  /**
   * Dynamically calculate the pagination for the GIF category groups tabs to
   * determine the number of tabs that can fit in each page. Existing
   * emoji/emoticon/symbol tabs pagination is hardcoded since the tabs remain
   * consistent, but GIF tabs change after every API call.
   */
  private setGifGroupsPagination(gifGroupTabs: GifSubcategoryData[]):
      GifSubcategoryData[] {
    const gifCategoriesGroupData = gifGroupTabs;

    // Max tabs bar width accounts for the chevron clicks and/or history button
    const maxTabsWidth = constants.EMOJI_PICKER_WIDTH -
        2 * constants.EMOJI_PICKER_SIDE_PADDING - 2 * constants.EMOJI_ICON_SIZE;

    let totalTabsWidth = 0;
    let pagination = 1;

    for (const tabData of gifCategoriesGroupData) {
      // Calculate maximum number of tabs that can fit within the tabs bar width
      // to set pagination
      const tabWidth = this.getTabElementWidth(tabData.name);
      if (totalTabsWidth + tabWidth > maxTabsWidth) {
        pagination += 1;
        totalTabsWidth = 0;
      }
      totalTabsWidth += tabWidth;
      tabData.pagination = pagination;
    }

    return gifCategoriesGroupData;
  }

  /**
   * Calculate the width of the specific tab element before it
   * is rendered by customising the dummy element with the tab name
   * and finding width.
   */
  private getTabElementWidth(tabName: string): number {
    this.dummyTab = {
      name: tabName,
      groupId: '-1',
      active: false,
      disabled: false,
      category: CategoryEnum.GIF,
    };

    return this.$.dummyTab.clientWidth;
  }

  /**
   * Returns true if the page is not the first.
   */
  private isNotFirstPage(pageNumber: number) {
    return pageNumber !== 1;
  }

  private getPaginationFromGroupId(groupId: string) {
    const tab = this.allCategoryTabs.find((tab) => tab.groupId === groupId);
    if (tab) {
      return tab.pagination;
    } else {
      throw new Error('Tab not found.');
    }
  }

  private calculateTabScrollLeftPosition(page: number) {
    const chevronMargin = constants.TAB_BUTTON_MARGIN;
    const offsetByLeftChevron = constants.EMOJI_ICON_SIZE + chevronMargin;
    return (page === 1) ?
        0 :
        (page - 1) * constants.EMOJI_PICKER_WIDTH - offsetByLeftChevron;
  }

  private getTabIndex(itemPagination: number, currentPagination: number) {
    return itemPagination === currentPagination ? 0 : -1;
  }

  // The gifSupport field ensures that this function gets called when
  // gifSupport is updated, allowing the correct categories to be shown
  private getCategoryMetadata(gifSupport: boolean, category: string) {
    // This determines whether the GIF category button will appear
    const METADATA = gifSupport ? GIF_CATEGORY_METADATA : CATEGORY_METADATA;
    return METADATA.map(data => ({
                          name: data.name,
                          icon: data.icon,
                          active: data.name === category,
                        }));
  }

  /**
   * Checks if recently used GIFs are still valid if we open the emoji picker
   * and it has been 24 hours since the last validation.
   */
  private async validateRecentlyUsedGifs() {
    // This check ensures that we don't try and validate recently used GIFs
    // if the validating process is already currently happening.
    const currentTime = new Date();

    if ((currentTime.getTime() - this.previousGifValidation.getTime()) >
        constants.TWENTY_FOUR_HOURS) {
      const updated = await this.categoriesHistory[CategoryEnum.GIF]?.validate(
          this.apiProxy);

      this.previousGifValidation = currentTime;
      window.localStorage.setItem(
          constants.GIF_VALIDATION_DATE, currentTime.toJSON());

      if (updated) {
        this.categoryHistoryUpdated(CategoryEnum.GIF);
      }
    }
  }

  private loadPreviousGifValidationTime(): Date {
    const stored = window.localStorage.getItem(constants.GIF_VALIDATION_DATE);
    if (!stored) {
      // First time opening the Emoji Picker so there should be no recently used
      // GIFs to render.
      return new Date();
    }
    return new Date(stored);
  }

  private computeListContainerClass(category: CategoryEnum, status: Status): string {
    // Only displays emoji-error if there is no internet connection and we are in GIF category.
    if (category === CategoryEnum.GIF && status !== Status.kHttpOk) {
      return 'error-only';
    }
    // Do not display GIF emoji groups if there is no internet connection and we are in non-GIF
    // category.
    if (category !== CategoryEnum.GIF && status !== Status.kHttpOk) {
      return 'no-gif';
    }
    return '';
  }

  private getLeftChevronAriaLabel(gifSupport: boolean): string | undefined {
    return gifSupport ? 'Previous' : undefined;
  }

  private getRightChevronAriaLabel(gifSupport: boolean): string | undefined {
    return gifSupport ? 'Next': undefined;
  }

  private closeGifNudgeOverlay() {
    if (this.showGifNudgeOverlay) {
      this.showGifNudgeOverlay = false;
    }

    GifNudgeHistoryStore.setNudgeShown(true);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [EmojiPickerApp.is]: EmojiPickerApp;
  }
  interface HTMLElementEventMap {
    [events.EMOJI_PICKER_READY]: CustomEvent;
    [events.CATEGORY_DATA_LOADED]: events.CategoryDataLoadEvent;
  }
}


customElements.define(EmojiPickerApp.is, EmojiPickerApp);
