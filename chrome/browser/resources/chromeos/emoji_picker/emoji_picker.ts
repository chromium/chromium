// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './icons.html.js';
import './emoji_group.js';
import './emoji_group_button.js';
import './emoji_search.js';
import './emoji_category_button.js';
import './text_group_button.js';
import 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';

import {CrSearchFieldElement} from 'chrome://resources/cr_elements/cr_search_field/cr_search_field.js';
import {afterNextRender, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import * as constants from './constants.js';
import {EmojiGroupComponent} from './emoji_group.js';
import {getTemplate} from './emoji_picker.html.js';
import {Feature} from './emoji_picker.mojom-webui.js';
import {EmojiPickerApiProxy, EmojiPickerApiProxyImpl} from './emoji_picker_api_proxy.js';
import {EmojiSearch} from './emoji_search.js';
import * as events from './events.js';
import {CATEGORY_METADATA, CATEGORY_TABS, EMOJI_GROUP_TABS, GIF_CATEGORY_METADATA, gifCategoryTabs, SUBCATEGORY_TABS, TABS_CATEGORY_START_INDEX, TABS_CATEGORY_START_INDEX_GIF_SUPPORT} from './metadata_extension.js';
import {RecentlyUsedStore} from './store.js';
import {CategoryEnum, Emoji, EmojiGroupData, EmojiGroupElement, EmojiVariants, GifSubcategoryData, SubcategoryData, VisualContent} from './types.js';

export class EmojiPicker extends PolymerElement {
  static get is() {
    return 'emoji-picker';
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
      dummyTab: {type: Object, value: () => ({})},
      categoriesData: {type: Array, value: () => ([])},
      categoriesGroupElements: {type: Array, value: () => ([])},
      activeInfiniteGroupElements: {type: Object, value: () => ({})},
      categoriesHistory: {type: Object, value: () => ({})},
      pagination: {type: Number, value: 1, observer: 'onPaginationChanged'},
      searchLazyIndexing: {type: Boolean, value: true},
      textSubcategoryBarEnabled: {
        type: Boolean,
        value: false,
        computed: 'isTextSubcategoryBarEnabled(category)',
        reflectToAttribute: true,
      },
      searchExtensionEnabled: {type: Boolean, value: false},
      incognito: {type: Boolean, value: true},
      gifSupport: {type: Boolean, value: false},
      gifDataInitialised: {type: Boolean, value: false},
    };
  }
  private category: CategoryEnum;
  private emojiGroupTabs: SubcategoryData[];
  private dummyTab: SubcategoryData;
  private allCategoryTabs: SubcategoryData[];
  categoriesData: EmojiGroupData;
  categoriesGroupElements: EmojiGroupElement[];
  activeInfiniteGroupElements: EmojiGroupElement;
  private categoriesHistory: {[index in CategoryEnum]: RecentlyUsedStore|null};
  private pagination: number;
  private searchLazyIndexing: boolean;
  private textSubcategoryBarEnabled: boolean;
  private searchExtensionEnabled: boolean;
  private incognito: boolean;
  private gifSupport: boolean;

  private scrollTimeout: number|null;
  private groupScrollTimeout: number|null;
  private groupButtonScrollTimeout: number|null;
  private activeVariant: EmojiGroupComponent|null;
  private apiProxy: EmojiPickerApiProxy;
  private autoScrollingToGroup: boolean;
  private highlightBarMoving: boolean;
  private groupTabsMoving: boolean;
  private gifDataInitialised: boolean;
  private previousGifValidation: Date;

  constructor() {
    super();

    // Incognito mode is set based on the default value.
    this.updateIncognitoState(this.incognito);

    this.emojiGroupTabs = EMOJI_GROUP_TABS;
    this.allCategoryTabs = SUBCATEGORY_TABS;

    this.scrollTimeout = null;
    this.groupScrollTimeout = null;
    this.groupButtonScrollTimeout = null;
    this.activeVariant = null;
    this.apiProxy = EmojiPickerApiProxyImpl.getInstance();
    this.autoScrollingToGroup = false;
    this.highlightBarMoving = false;
    this.groupTabsMoving = false;
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
  }

  private filterGroupTabByPagination(pageNumber: number) {
    return function(tab: {pagination: number, groupId: string}) {
      return tab.pagination === pageNumber && !tab.groupId.includes('history');
    };
  }

  initHistoryUi(incognito: boolean) {
    if (incognito !== this.incognito) {
      this.updateIncognitoState(incognito);
    }
    this.updateHistoryTabDisabledProperty();
    // Make highlight bar visible (now we know where it should be) and
    // add smooth sliding.
    this.updateActiveGroup();
    const bar = this.getBar();
    if (bar) {
      bar.style.display = 'block';
      bar.style.transition = 'left 200ms';
    }
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

    const dataUrls = EmojiPicker.configs().dataUrls;
    // Create an ordered list of category and urls based on the order that
    // categories need to appear in the UIs.
    const categoryDataUrls =
        METADATA.filter((item) => dataUrls[item.name])
            .map(
                item => ({'category': item.name, 'urls': dataUrls[item.name]}));

    // Fetch and process all the data.
    this.fetchAndProcessData(categoryDataUrls);

    this.updateStyles({
      '--emoji-group-button-size': constants.EMOJI_GROUP_SIZE_PX,
      '--emoji-picker-width': constants.EMOJI_PICKER_WIDTH_PX,
      '--emoji-picker-height': constants.EMOJI_PICKER_HEIGHT_PX,
      '--emoji-size': constants.EMOJI_SIZE_PX,
      '--emoji-per-row': constants.EMOJI_PER_ROW,
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

  /**
   * Fetches data and updates all the variables that are required to render
   * EmojiPicker UI. This function serves as the main entry for creating and
   * managing async calls dealing with fetching data and rendering UI in the
   * correct order. These include:
   *   * Feature list
   *   * Incognito state
   *   * Category data (emoji, emoticon, etc.)
   */
  async fetchAndProcessData(
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

    this.updateStyles({
      '--emoji-size': constants.EMOJI_ICON_SIZE_PX,
      '--emoji-spacing': constants.EMOJI_SPACING_PX,
    });

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

    let prevFetchPromise = Promise.resolve();
    let prevRenderPromise = Promise.resolve();

    // Create a chain of promises for fetching and rendering data of
    // different categories in the correct order.
    remainingData.forEach(
        (dataUrl, index) => {
          // Fetch the url only after the previous url is fetched.
          prevFetchPromise =
              prevFetchPromise.then(
                  () => this.fetchOrderingData(dataUrl.url)) as Promise<void>;

          // Update category data after the data is fetched and the previous
          // category data update/rendering completed successfully.
          prevRenderPromise =
              Promise
                  .all(
                      [prevRenderPromise, prevFetchPromise],
                      )
                  // Hacky cast below, but should be safe
                  .then((values) => values[1] as unknown as EmojiGroupData)
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
      this.validateRecentlyUsedGifs();
      const categoriesFetchPromise =
          prevFetchPromise.then(() => this.apiProxy.getCategories());

      const categoriesRenderPromise =
          Promise.all([prevRenderPromise, categoriesFetchPromise])
              .then((values) => {
                const {gifCategories} = values[1];
                const categoryTabs = {
                  ...CATEGORY_TABS,
                  gif: this.setGifGroupsPagination(
                      [{name: constants.TRENDING}, ...gifCategories]),
                };
                this.allCategoryTabs = gifCategoryTabs(categoryTabs);
              });

      const featuredGifFetchPromise =
          categoriesFetchPromise.then(() => this.apiProxy.getFeaturedGifs());
      Promise.all([categoriesRenderPromise, featuredGifFetchPromise])
          .then((values) => {
            const {featuredGifs} = values[1];
            const trendingGifsElement: EmojiVariants[] =
                this.apiProxy.convertTenorGifsToEmoji(featuredGifs);

            const trendingGifs = [{
              group: constants.TRENDING,
              category: CategoryEnum.GIF,
              emoji: trendingGifsElement,
            }];

            this.gifDataInitialised = true;

            this.updateCategoryData(trendingGifs, CategoryEnum.GIF);
            const trendingCategoryIndex = this.allCategoryTabs.findIndex(
                tab => tab.name === constants.TRENDING);
            this.activeInfiniteGroupElements = this.createEmojiGroupElement(
                trendingGifsElement, {}, false, trendingCategoryIndex);
          });
    }
  }

  isInfinite(category: CategoryEnum) {
    return category === CategoryEnum.GIF;
  }

  setActiveFeatures(featureList: Feature[]) {
    this.searchExtensionEnabled =
        featureList.includes(Feature.EMOJI_PICKER_SEARCH_EXTENSION);
    this.gifSupport = featureList.includes(Feature.EMOJI_PICKER_GIF_SUPPORT);
  }

  fetchOrderingData(url: string): Promise<EmojiGroupData> {
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
   * @param {!EmojiGroupData} data The category data to be processes.
   *    Note: category field will be added to the each EmojiGroup in data.
   * @param {!CategoryEnum} category Category of the data.
   * @param {boolean} categoryLastPartition True if no future data updates
   *      are expected for the given category.
   * @param {boolean} lastPartition True if no future data updates are
   *      expected.
   *
   * @fires CustomEvent#`EMOJI_PICKER_READY`
   * @fires CustomEvent#`CATEGORY_DATA_LOADED``
   */
  updateCategoryData(
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
    this.push('categoriesData', ...data);
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
          () => {
            this.dispatchEvent(
                events.createCustomEvent(events.EMOJI_PICKER_READY, {}));
          },
      );
    }
  }

  onSearchChanged(newValue: string) {
    const elem = this.$['list-container'] as HTMLElement | null;
    if (elem) {
      elem.style.display = newValue ? 'none' : '';
    }
  }

  onBarTransitionStart() {
    this.highlightBarMoving = true;
  }

  onBarTransitionEnd() {
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

  async insertText(category: CategoryEnum, item: {
    emoji: string,
    isVariant: boolean,
    baseEmoji: string,
    allVariants?: Emoji[], name: string, text: string,
  }) {
    const {text, isVariant, baseEmoji, allVariants, name} = item;
    const message = this.getMessage();
    if (message) {
      message.textContent = text + ' inserted.';
    }

    this.insertHistoryTextItem(category, {
      selectedEmoji: text,
      baseEmoji: baseEmoji,
      alternates: allVariants || [],
      name: name,
    });

    const searchLength = (this.getSearchContainer()?.shadowRoot?.querySelector(
                              'cr-search-field') as CrSearchFieldElement)
                             .getSearchInput()
                             .value.length;

    // TODO(b/217276960): change to a more generic name
    this.apiProxy.insertEmoji(text, isVariant, searchLength);
  }

  insertVisualContent(category: CategoryEnum, item: {
    name: string,
    visualContent: VisualContent,
  }) {
    this.apiProxy.copyGifToClipboard(item.visualContent.url.full);
    this.insertHistoryVisualContentItem(category, item);
  }

  clearRecentEmoji(event: events.EmojiClearRecentClickEvent) {
    const category = event.detail.category;
    this.clearHistoryData(category);
    afterNextRender(this, () => {
      this.updateActiveGroup();
      this.updateHistoryTabDisabledProperty();
    });
  }

  selectTextEmojiGroup(newGroup: string) {
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

  async setGifGroupElements(activeGroupId: string) {
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

      const activeCategoryIndex =
          this.allCategoryTabs.findIndex(tab => tab.groupId === activeGroupId);

      gifGroupElements = this.createEmojiGroupElement(
          gifElements, {}, false, activeCategoryIndex);
    }

    this.setActiveInfiniteGroupElements(activeGroupId, gifGroupElements);
    if (!isCached) {
      this.categoriesGroupElements =
          [...this.categoriesGroupElements, gifGroupElements];
    }
  }

  setActiveInfiniteGroupElements(
      activeGroupId: string, gifGroupElements: EmojiGroupElement) {
    this.updateActiveGroup(activeGroupId);
    this.activeInfiniteGroupElements = gifGroupElements;
  }

  async selectGroup(newGroup: string) {
    if (this.category === CategoryEnum.GIF) {
      this.setGifGroupElements(newGroup);
    } else {
      this.selectTextEmojiGroup(newGroup);
    }
  }

  onEmojiScroll() {
    // the scroll event is fired very frequently while scrolling.
    // only update active tab 100ms after last scroll event by setting
    // a timeout.
    if (this.scrollTimeout) {
      clearTimeout(this.scrollTimeout);
    }
    this.scrollTimeout = setTimeout(() => {
      // Active category and group will not change via scrolling
      // for infinite emoji types e.g. GIFs
      if (!this.isInfinite(this.category)) {
        this.updateActiveCategory();
        this.updateActiveGroup();
      }
    }, 100);
  }

  onRightChevronClick() {
    if (!this.textSubcategoryBarEnabled) {
      // ! safe due to &&
      this.getTabs() &&
          (this.getTabs()!.scrollLeft =
               constants.EMOJI_PICKER_TOTAL_EMOJI_WIDTH *
               constants.EMOJI_NUM_TABS_IN_FIRST_PAGE);
      this.scrollToGroup(
          EMOJI_GROUP_TABS[constants.GROUP_PER_ROW - 1]?.groupId);
      this.groupTabsMoving = true;
      // ! safe due to &&
      this.getBar() &&
          (this.getBar()!.style.left =
               constants.EMOJI_PICKER_TOTAL_EMOJI_WIDTH_PX);
    } else {
      const maxPagination =
          this.getPaginationArray(this.emojiGroupTabs).pop() ?? 0;
      this.pagination = Math.min(this.pagination + 1, maxPagination);
      this.updateCurrentGroupTabs();
    }
  }

  onLeftChevronClick() {
    this.pagination = Math.max(this.pagination - 1, 1);
    this.updateCurrentGroupTabs();
  }

  updateCurrentGroupTabs() {
    const nextTab =
        this.emojiGroupTabs.find((tab) => tab.pagination === this.pagination);
    if (this.category === CategoryEnum.GIF) {
      // GIF group tabs movement isn't affected by scrolling
      this.groupTabsMoving = false;

      // TODO(b/265731647) Set the GIF elements for the first GIF tab group
      // Awaiting (b/263920562) to be merged in
    } else {
      this.scrollToGroup(nextTab?.groupId);
      this.groupTabsMoving = true;
    }
  }

  scrollToGroup(newGroup?: string) {
    // TODO(crbug/1152237): This should use behaviour:'smooth', but when you do
    // that it doesn't scroll.
    if (newGroup) {
      this.shadowRoot?.querySelector(`div[data-group="${newGroup}"]`)
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

    this.groupTabsMoving = true;

    if (this.groupButtonScrollTimeout) {
      clearTimeout(this.groupButtonScrollTimeout);
    }
    this.groupButtonScrollTimeout =
        setTimeout(this.groupTabScrollFinished.bind(this), 100);
  }

  private groupTabScrollFinished() {
    this.groupTabsMoving = false;
    this.updateActiveGroup();
  }

  private updateChevrons() {
    const leftChevron = this.$['left-chevron'] as HTMLElement | null;
    const rightChevron = this.$['right-chevron'] as HTMLElement | null;
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
   * @returns {string} the id of the emoji or emoticon group currently in view.
   * This does not apply to GIFs, since each infinite set of GIF elements for
   * the GIF categories are displayed on a separate page, and cannot be
   * accessible from other categories via scrolling.
   */
  getActiveGroupIdFromScrollPosition() {
    // get bounding rect of scrollable emoji region.
    const thisRect = this.$['groups']?.getBoundingClientRect();
    if (!thisRect) {
      return '';
    }

    const groupElements =
        Array.from(this.$['groups']?.querySelectorAll('[data-group]') ?? []);

    // activate the first group which is visible for at least 20 pixels,
    // i.e. whose bottom edge is at least 20px below the top edge of the
    // scrollable region.
    const activeGroup =
        groupElements.find(
            el => el.getBoundingClientRect().bottom - thisRect.top >= 20) as
            HTMLElement |
        null;

    const activeGroupId =
        activeGroup ? activeGroup.dataset['group'] ?? '' : 'emoji-history';

    return activeGroupId;
  }

  /**
   * Active group is updated with scroll position for emoji types with finite
   * elements (emojis, emoticons, symbols). However, active groups for emoji
   * types with infinite will be passed through a groupId because they cannot be
   * determined with scroll position.
   */
  updateActiveGroup(groupId?: string) {
    let activeGroupId = groupId;
    if (activeGroupId == null && this.category !== CategoryEnum.GIF) {
      activeGroupId = this.getActiveGroupIdFromScrollPosition();
    } else if (activeGroupId == null && this.category === CategoryEnum.GIF) {
      // Set the default active group to be trending
      // TODO(b/265594436): Display correct first group
      // (account for recently used if there are history items)
      activeGroupId =
          this.categoriesGroupElements
              .find(
                  groupElement => groupElement.category === CategoryEnum.GIF &&
                      groupElement.name === constants.TRENDING)
              ?.groupId;
    }
    this.set(
        'pagination', this.getPaginationFromGroupId(activeGroupId as string));
    this.updateChevrons();
    const bar = this.getBar();

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
    if (!this.highlightBarMoving && !this.groupTabsMoving) {
      // Update the scroll position of the emoji groups so that active group is
      // visible.
      if (!this.textSubcategoryBarEnabled) {
        if (bar) {
          bar.style.width = constants.EMOJI_HIGHLIGHTER_WIDTH_PX;
          bar.style.left =
              `${index * constants.EMOJI_PICKER_TOTAL_EMOJI_WIDTH}px`;
        }
      } else {
        // Cast below should be safe as worst case array is empty.
        const subcategoryTabs =
            Array.from(this.getTabs()?.getElementsByClassName('tab') ?? []) as
            HTMLElement[];

        // for text group button, the highlight bar only spans its inner width,
        // which excludes both padding and margin.
        if (index < subcategoryTabs.length) {
          const barInlineGap =
              constants.TAB_BUTTON_MARGIN + constants.TEXT_GROUP_BUTTON_PADDING;
          const currentTab = subcategoryTabs[index];
          if (bar) {
            bar.style.left = `${
                (currentTab?.offsetLeft ?? 0) -
                constants.EMOJI_PICKER_SIDE_PADDING -
                this.calculateTabScrollLeftPosition(this.pagination)}px`;
            bar.style.width = `${
                (subcategoryTabs[index]?.clientWidth ?? 0) -
                barInlineGap * 2}px`;
          }
        } else if (bar) {
          bar.style.left = '0px';
          bar.style.width = '0px';
        }
      }
    }
  }

  /**
   * Update active category by using vertical scroll position.
   */
  updateActiveCategory() {
    const activeGroupId = this.getActiveGroupIdFromScrollPosition();

    const currentCategory =
        this.allCategoryTabs.find((tab) => tab.groupId === activeGroupId)
            ?.category;
    this.set('category', currentCategory);
  }

  preventV2TabScrolling() {
    const tabs = this.getTabs();
    if (tabs) {
      tabs.scrollLeft = this.calculateTabScrollLeftPosition(this.pagination);
    }
  }

  hideDialogs() {
    this.hideEmojiVariants();

    for (const category of Object.values(CategoryEnum)) {
      if (!this.isCategoryHistoryEmpty(category)) {
        const historyButton =
            this.shadowRoot?.querySelector<EmojiGroupComponent>(
                `emoji-group.history[category=${category}]`);
        if (historyButton) {
          historyButton.showClearRecents = false;
        }
      }
    }
  }

  hideEmojiVariants() {
    if (this.activeVariant) {
      this.activeVariant.hideEmojiVariants();
      this.activeVariant = null;
    }
  }

  /**
   * Disables the history tab when there is no usage history for the
   * selected category and enables it otherwise.
   */
  updateHistoryTabDisabledProperty() {
    this.set(
        ['emojiGroupTabs', 0, 'disabled'],
        this.isCategoryHistoryEmpty(this.category),
    );
  }

  /**
   * Gets recently used emojis for a category. It gets the history items
   * and convert them to emojis.
   */
  getHistoryEmojis(category: CategoryEnum): EmojiVariants[] {
    if (this.incognito) {
      return [];
    }

    return this.categoriesHistory[category]?.data?.history?.map(
               emoji => ({
                 base: {
                   string: emoji.base.string,
                   name: emoji.base.name,
                   visualContent: emoji.base.visualContent,
                   keywords: [],
                 },
                 alternates: emoji.alternates.map(
                     (alternate: Emoji):
                         Emoji => {
                           return {
                             string: alternate.string,
                             name: alternate.name,
                             keywords: [...(alternate.keywords ?? [])],
                           };
                         }),
               })) ??
        [];
  }

  /**
   * Handles the event where history or preferences are modified for a
   * category.
   *
   */
  categoryHistoryUpdated(
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
  updateIncognitoState(incognito: boolean) {
    this.incognito = incognito;
    // Load the history item for each category.
    for (const category of Object.values(CategoryEnum)) {
      this.categoriesHistory[category] =
          incognito ? null : new RecentlyUsedStore(`${category}-recently-used`);
      this.categoryHistoryUpdated(category);
    }
  }

  /**
   * Inserts a new item to the history of a category. It will do nothing during
   * incognito state.
   *
   */
  insertHistoryTextItem(category: CategoryEnum, item: {
    selectedEmoji: string,
    baseEmoji: string,
    alternates: Emoji[],
    name: string,
  }) {
    if (this.incognito) {
      return;
    }

    const {selectedEmoji, baseEmoji, alternates, name} = item;

    this.categoriesHistory[category]?.bumpItem(
        category,
        {base: {string: selectedEmoji, name: name}, alternates: alternates});

    const preferenceUpdated =
        this.categoriesHistory[category]?.savePreferredVariant(
            baseEmoji, selectedEmoji);

    this.categoryHistoryUpdated(category, true, preferenceUpdated);
  }

  /**
   * Inserts a new item to the history of a visual content category. It will do
   * nothing during incognito state.
   */
  insertHistoryVisualContentItem(category: CategoryEnum, item: {
    visualContent: VisualContent,
    name: string,
  }) {
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
   * Clears history items for a category.
   */
  clearHistoryData(category: CategoryEnum) {
    if (this.incognito) {
      return;
    }

    this.categoriesHistory[category]?.clearRecents();
    this.categoryHistoryUpdated(category, true, false);
  }

  /**
   * Check if the history items of a category is empty.
   *
   * @returns {boolean} True for empty history.
   */
  isCategoryHistoryEmpty(category: CategoryEnum) {
    return this.incognito ||
        this.categoriesHistory[category]?.data?.history?.length == 0;
  }

  /**
   * Gets HTML classes for an emoji group element.
   *
   * @returns {string} HTML element class attribute.
   */
  getEmojiGroupClassNames(isHistory: boolean, emojis: EmojiVariants[]) {
    const baseClassNames = isHistory ? 'group history' : 'group';

    // Make emoji hidden if it is empty.
    // Note: Filtering empty groups in dom-repeat is expensive due to
    // re-rendering, so making it hidden is more efficient.
    if (!emojis || emojis.length === 0) {
      return baseClassNames + ' hidden';
    }
    return baseClassNames;
  }

  /**
   * Create an instance of emoji group element.
   *
   * @returns {EmojiGroupElement} Instance of emoji group element.
   */
  createEmojiGroupElement(
      emoji: EmojiVariants[], preferences: {[index: string]: string},
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
  getEmojiGroupPreference(category: CategoryEnum): {[index: string]: string} {
    return this.incognito ? {} :
                            // ! is safe as categories history must contain
                            // entries for all categories.
                            this.categoriesHistory[category]!.data.preference;
  }

  onShowEmojiVariants(ev: events.EmojiVariantsShownEvent) {
    // Hide the currently shown emoji variants if the new one belongs
    // to a different emoji group.
    if (this.activeVariant && ev.detail.owner !== this.activeVariant) {
      this.hideEmojiVariants();
    }

    this.activeVariant = ev.detail.owner as EmojiGroupComponent;

    // Updates the UI if a variant is shown.
    if (ev.detail.variants) {
      const message = this.getMessage();
      if (message) {
        message.textContent = ev.detail.baseEmoji + ' variants shown.';
      }
      this.positionEmojiVariants(ev.detail.variants);
    }
  }

  positionEmojiVariants(variants: HTMLElement) {
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
    const groups = this.$['groups'] as HTMLElement | null;
    const scrollTop = groups?.scrollTop ?? 0;
    const variantTop = variants?.offsetTop ?? 0;
    const variantBottom = variantTop + variants.offsetHeight;
    const listTop =
        (this.$['list-container'] as HTMLElement | null)?.offsetTop ?? 0;
    if (variantBottom > scrollTop + (groups?.offsetHeight ?? 0) + listTop) {
      groups?.scrollTo({
        top: variantBottom - (groups?.offsetHeight ?? 0) - listTop,
        left: 0,
        behavior: 'smooth',
      });
    }
  }

  /**
   * Triggers when category property changes
   */
  onCategoryChanged(newCategoryName: string) {
    const categoryTabs =
        this.allCategoryTabs.filter(tab => tab.category === newCategoryName);
    this.set('emojiGroupTabs', categoryTabs);
    afterNextRender(this, () => {
      this.updateActiveGroup(
          this.allCategoryTabs.find(tab => tab.category === newCategoryName)
              ?.groupId);
      this.updateHistoryTabDisabledProperty();
      const tabs = this.getTabs();
      if (tabs) {
        tabs.scrollLeft = this.calculateTabScrollLeftPosition(this.pagination);
      }
    });
  }

  onCategoryButtonClick(newCategory: CategoryEnum) {
    // TODO(b/264485361): Change the below if statement so that some sort of
    // indication that GIFs are still loading appears on the Emoji Picker
    // instead of not allowing the user to change to the GIF section
    if (newCategory === CategoryEnum.GIF && !this.gifDataInitialised) {
      return;
    }

    this.set('category', newCategory);
    this.set('pagination', 1);

    // Jump to the GIF section instead of scrolling down
    if (newCategory === CategoryEnum.GIF) {
      // Currently displays trending items when first clicking on GIF category
      // TODO(b/265594436): Display correct first group
      // (account for recently used if there are history items)
      const trendingGifElements = this.categoriesGroupElements.find(
          groupElement => groupElement.category === CategoryEnum.GIF &&
              groupElement.name === constants.TRENDING);
      if (trendingGifElements) {
        this.updateActiveGroup(trendingGifElements.groupId);
        this.activeInfiniteGroupElements = trendingGifElements;
      }
    } else {
      // Scroll to relevant category elements if emojis, emoticons or symbols
      if (this.getSearchContainer()?.searchNotEmpty()) {
        this.getSearchContainer()?.setSearchQuery('');
        afterNextRender(this, () => {
          this.scrollToGroup(this.emojiGroupTabs[0]?.groupId);
        });
      } else {
        this.scrollToGroup(this.emojiGroupTabs[0]?.groupId);
      }
    }
  }

  /**
   * Trigger when pagination changes
   */
  onPaginationChanged(newPage: number) {
    const tabs = this.getTabs();
    if (tabs) {
      // Left chevron has the same margin as the text subcategory button.
      tabs.scrollLeft = this.calculateTabScrollLeftPosition(newPage);
    }
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

    const dummyTab = this.getDummy();
    const dummyTabWidth = dummyTab ? dummyTab.clientWidth : 0;

    return dummyTabWidth;
  }

  /**
   * Returns true if the page is not the first.
   */
  private isNotFirstPage(pageNumber: number) {
    return pageNumber !== 1;
  }

  getPaginationFromGroupId(groupId: string) {
    const tab = this.allCategoryTabs.find((tab) => tab.groupId === groupId);
    if (tab) {
      return tab.pagination;
    } else {
      throw new Error('Tab not found.');
    }
  }

  calculateTabScrollLeftPosition(page: number) {
    const chevronMargin = constants.TAB_BUTTON_MARGIN;
    const offsetByLeftChevron = constants.EMOJI_ICON_SIZE + chevronMargin;
    return (page === 1) ?
        0 :
        (page - 1) * constants.EMOJI_PICKER_WIDTH - offsetByLeftChevron;
  }

  getTabIndex(itemPagination: number, currentPagination: number) {
    return itemPagination === currentPagination ? 0 : -1;
  }

  // The gifSupport field ensures that this function gets called when
  // gifSupport is updated, allowing the correct categories to be shown
  getCategoryMetadata(gifSupport: boolean, category: string) {
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
  async validateRecentlyUsedGifs() {
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

  private getTabs() {
    return this.$['tabs'] as HTMLElement | null;
  }

  private getDummy() {
    return this.$['dummyTab'] as HTMLElement | null;
  }

  private getSearchContainer() {
    return this.$['search-container'] as EmojiSearch | null;
  }

  private getMessage() {
    return this.$['message'] as HTMLElement | null;
  }

  private getBar() {
    return this.$['bar'] as HTMLElement | null;
  }
}

customElements.define(EmojiPicker.is, EmojiPicker);
