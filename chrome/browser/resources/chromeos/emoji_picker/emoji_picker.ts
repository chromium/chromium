// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './icons.html.js';
import './emoji_group.js';
import './emoji_group_button.js';
import './emoji_search.js';
import './emoji_category_button.js';
import './text_group_button.js';
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
import {CATEGORY_METADATA, EMOJI_GROUP_TABS, V2_SUBCATEGORY_TABS, V2_TABS_CATEGORY_START_INDEX} from './metadata_extension.js';
import {RecentlyUsedStore} from './store.js';
import {CategoryEnum, EmojiGroupData, EmojiGroupElement, EmojiVariants, SubcategoryData} from './types.js';

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
      },
    };
  }

  static get properties() {
    return {
      category: {type: String, value: 'emoji', observer: 'onCategoryChanged'},
      emojiGroupTabs: {type: Array},
      categoriesData: {type: Array, value: () => ([])},
      categoriesGroupElements: {type: Array, value: () => ([])},
      categoriesHistory: {type: Object, value: () => ({})},
      pagination: {type: Number, value: 1, observer: 'onPaginationChanged'},
      searchLazyIndexing: {type: Boolean, value: true},
      textSubcategoryBarEnabled: {
        type: Boolean,
        value: false,
        computed: 'isTextSubcategoryBarEnabled(v2Enabled, category)',
        reflectToAttribute: true,
      },
      v2Enabled: {type: Boolean, value: false, reflectToAttribute: true},
      searchExtensionEnabled: {type: Boolean, value: false},
      incognito: {type: Boolean, value: true},
      gifSupport: {type: Boolean, value: false},
    };
  }
  private category: CategoryEnum;
  private emojiGroupTabs: SubcategoryData[];
  categoriesData: EmojiGroupData;
  categoriesGroupElements: EmojiGroupElement[];
  private categoriesHistory: {[index in CategoryEnum]: RecentlyUsedStore|null};
  private pagination: number;
  private searchLazyIndexing: boolean;
  private textSubcategoryBarEnabled: boolean;
  private v2Enabled: boolean;
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

  constructor() {
    super();

    // Incognito mode is set based on the default value.
    this.updateIncognitoState(this.incognito);

    this.emojiGroupTabs = EMOJI_GROUP_TABS;

    this.scrollTimeout = null;
    this.groupScrollTimeout = null;
    this.groupButtonScrollTimeout = null;
    this.activeVariant = null;
    this.apiProxy = EmojiPickerApiProxyImpl.getInstance();
    this.autoScrollingToGroup = false;
    this.highlightBarMoving = false;
    this.groupTabsMoving = false;

    this.addEventListener(
        events.GROUP_BUTTON_CLICK,
        (ev: events.GroupButtonClickEvent) =>
            this.selectGroup(ev.detail.group));
    this.addEventListener(
        events.EMOJI_BUTTON_CLICK,
        (ev: events.EmojiButtonClickEvent) => this.onEmojiButtonClick(ev));
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
    this.updateActiveGroup(/*updateTabsScroll=*/ true);
    const bar = this.getBar();
    if (bar) {
      bar.style.display = 'block';
      bar.style.transition = 'left 200ms';
    }
  }

  override ready() {
    super.ready();

    // Ensure first category is emoji for compatibility with V1.
    if (CATEGORY_METADATA[0]?.name !== CategoryEnum.EMOJI) {
      throw new Error(`First category is ${
          CATEGORY_METADATA[0]?.name} but must be 'emoji'.`);
    }

    const dataUrls = EmojiPicker.configs().dataUrls;
    // Create an ordered list of category and urls based on the order that
    // categories need to appear in the UIs.
    const categoryDataUrls =
        CATEGORY_METADATA.filter((item) => dataUrls[item.name])
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
      '--v2-emoji-picker-width': constants.V2_EMOJI_PICKER_WIDTH_PX,
      '--v2-emoji-picker-height': constants.V2_EMOJI_PICKER_HEIGHT_PX,
      '--v2-emoji-picker-side-padding':
          constants.V2_EMOJI_PICKER_SIDE_PADDING_PX,
      '--v2-emoji-group-spacing': constants.V2_EMOJI_GROUP_SPACING_PX,
      '--v2-tab-button-margin': constants.V2_TAB_BUTTON_MARGIN_PX,
      '--v2-text-group-button-padding':
          constants.V2_TEXT_GROUP_BUTTON_PADDING_PX,
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

    if (this.v2Enabled) {
      this.updateStyles({
        '--emoji-size': constants.V2_EMOJI_ICON_SIZE_PX,
        '--emoji-spacing': constants.V2_EMOJI_SPACING_PX,
      });
    }

    // Update UI and relevant features based on the initial data.
    this.updateCategoryData(
        // If we don't have 1 data URL, a crash probably isn't a bad idea
        initialData, dataUrls[0]!.category, dataUrls[0]!.categoryLastPartition,
        !this.v2Enabled && dataUrls[0]!.categoryLastPartition);

    // Show the UI after the initial data is rendered.
    afterNextRender(this, () => {
      this.apiProxy.showUi();
    });

    // Filter data urls based on the version. Remove the first url as it is
    // already added and shown.
    const remainingData = this.v2Enabled ?
        dataUrls.slice(1) :
        dataUrls.slice(1).filter(
            item => item.category === dataUrls[0]?.category);

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
  }

  setActiveFeatures(featureList: Feature[]) {
    this.v2Enabled = featureList.includes(Feature.EMOJI_PICKER_EXTENSION);
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
    const startIndex = V2_TABS_CATEGORY_START_INDEX.get(category);
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
      const tabCategory = V2_SUBCATEGORY_TABS[tabIndex]?.category;
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
      const dataMatchSubcategoryTabs = this.v2Enabled ?
          numEmojiGroups === V2_SUBCATEGORY_TABS.length :
          V2_SUBCATEGORY_TABS[numEmojiGroups]?.category !== CategoryEnum.EMOJI;

      // Ensure hard-coded tabs match the loaded data.
      console.assert(
          dataMatchSubcategoryTabs,
          `The Number of tabs "${V2_SUBCATEGORY_TABS.length}" does not match ` +
              ` the number of loaded groups "${numEmojiGroups}".`,
      );

      afterNextRender(
          this,
          () => {
            this.dispatchEvent(events.createCustomEvent(
                events.EMOJI_PICKER_READY, {'v2Enabled': this.v2Enabled}));
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

  private onEmojiButtonClick(ev: events.EmojiButtonClickEvent) {
    const category = ev.detail.category;
    this.insertText(category, ev.detail);
  }

  async insertText(category: CategoryEnum, item: {
    emoji: string,
    isVariant: boolean,
    baseEmoji: string,
    allVariants?: string[], name: string, text: string,
  }) {
    const {text, isVariant, baseEmoji, allVariants, name} = item;
    const message = this.getMessage();
    if (message) {
      message.textContent = text + ' inserted.';
    }

    this.insertHistoryItem(category, {
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

  clearRecentEmoji(event: events.EmojiClearRecentClickEvent) {
    const category = event.detail.category;
    this.clearHistoryData(category);
    afterNextRender(this, () => {
      this.updateActiveGroup(/*updateTabsScroll=*/ true);
      this.updateHistoryTabDisabledProperty();
    });
  }

  selectGroup(newGroup: string) {
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

  onEmojiScroll() {
    // the scroll event is fired very frequently while scrolling.
    // only update active tab 100ms after last scroll event by setting
    // a timeout.
    if (this.scrollTimeout) {
      clearTimeout(this.scrollTimeout);
    }
    this.scrollTimeout = setTimeout(() => {
      this.updateActiveCategory();
      this.updateActiveGroup(/*updateTabsScroll=*/ true);
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

      const nextTab =
          this.emojiGroupTabs.find((tab) => tab.pagination === this.pagination);
      this.scrollToGroup(nextTab?.groupId);
      this.groupTabsMoving = true;
    }
  }

  onLeftChevronClick() {
    if (!this.v2Enabled) {
      // ! is safe here due to &&
      this.getTabs() && (this.getTabs()!.scrollLeft = 0);
      this.scrollToGroup(EMOJI_GROUP_TABS[0]?.groupId);
      this.groupTabsMoving = true;
      const bar = this.getBar();
      if (bar) {
        if (!this.isCategoryHistoryEmpty(CategoryEnum.EMOJI)) {
          bar.style.left = '0';
        } else {
          bar.style.left = constants.EMOJI_PICKER_TOTAL_EMOJI_WIDTH_PX;
        }
      }
    } else {
      this.pagination = Math.max(this.pagination - 1, 1);

      const nextTab =
          this.emojiGroupTabs.find((tab) => tab.pagination === this.pagination);
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
    this.groupTabsMoving = true;

    if (this.groupButtonScrollTimeout) {
      clearTimeout(this.groupButtonScrollTimeout);
    }
    this.groupButtonScrollTimeout =
        setTimeout(this.groupTabScrollFinished.bind(this), 100);
  }

  private groupTabScrollFinished() {
    this.groupTabsMoving = false;
    this.updateActiveGroup(/*updateTabsScroll=*/ false);
  }

  private updateChevrons() {
    const leftChevron = this.$['left-chevron'] as HTMLElement | null;
    const rightChevron = this.$['right-chevron'] as HTMLElement | null;
    // bail early if required elements don't exist
    if (!leftChevron || !rightChevron) {
      return;
    }
    if (!this.v2Enabled) {
      if (this.getTabs()?.scrollLeft ?? 0 > constants.GROUP_ICON_SIZE) {
        leftChevron.style.display = 'flex';
      } else {
        leftChevron.style.display = 'none';
      }
      // 1 less because we need to allow room for the chevrons
      if (this.getTabs()?.scrollLeft ??
          0 + constants.GROUP_ICON_SIZE * constants.GROUP_PER_ROW <
              constants.GROUP_ICON_SIZE * (EMOJI_GROUP_TABS.length + 1)) {
        rightChevron.style.display = 'flex';
      } else {
        rightChevron.style.display = 'none';
      }
    } else if (this.v2Enabled && !this.textSubcategoryBarEnabled) {
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

  updateActiveGroup(updateTabsScroll: boolean) {
    const activeGroupId = this.getActiveGroupIdFromScrollPosition();
    this.set('pagination', this.getPaginationFromGroupId(activeGroupId));
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
      if (!this.v2Enabled) {
        // for emoji group buttons, their highlighter always has a fixed width.
        // ! is safe due to initial check.
        bar && (bar!.style.width = constants.EMOJI_HIGHLIGHTER_WIDTH_PX);

        // TODO(b/213120632): Convert the following number literals into
        // contextualized constants.
        let tabscrollLeft = this.getTabs()?.scrollLeft ?? 0;
        if (tabscrollLeft >
            constants.EMOJI_PICKER_TOTAL_EMOJI_WIDTH * (index - 0.5)) {
          tabscrollLeft = 0;
        }
        if (tabscrollLeft +
                constants.EMOJI_PICKER_TOTAL_EMOJI_WIDTH *
                    (constants.GROUP_PER_ROW - 2) <
            constants.GROUP_ICON_SIZE * index) {
          // 5 = We want the seventh icon to be first. Then -1 for chevron, -1
          // for 1 based indexing.
          tabscrollLeft = constants.EMOJI_PICKER_TOTAL_EMOJI_WIDTH *
              (constants.EMOJI_NUM_TABS_IN_FIRST_PAGE - 1);
        }

        if (bar) {
          if (updateTabsScroll) {
            bar.style.left = (index * constants.EMOJI_PICKER_TOTAL_EMOJI_WIDTH -
                              tabscrollLeft) +
                'px';
          } else {
            bar.style.left = (index * constants.EMOJI_PICKER_TOTAL_EMOJI_WIDTH -
                              (this.getTabs()?.scrollLeft ?? 0)) +
                'px';
          }
        }
        // only update tab scroll when using emoji-based subcategory, because
        // the scroll position of the text-based subcategory bar is controlled
        // differently.
        if (updateTabsScroll && !this.textSubcategoryBarEnabled) {
          this.getTabs() && (this.getTabs()!.scrollLeft = tabscrollLeft);
        }
      } else if (this.v2Enabled && !this.textSubcategoryBarEnabled) {
        if (bar) {
          bar.style.width = constants.EMOJI_HIGHLIGHTER_WIDTH_PX;
          bar.style.left =
              `${index * constants.V2_EMOJI_PICKER_TOTAL_EMOJI_WIDTH}px`;
        }
      } else {
        // Cast below should be safe as worst case array is empty.
        const subcategoryTabs =
            Array.from(this.getTabs()?.getElementsByClassName('tab') ?? []) as
            HTMLElement[];

        // for text group button, the highlight bar only spans its inner width,
        // which excludes both padding and margin.
        if (index < subcategoryTabs.length) {
          const barInlineGap = constants.V2_TAB_BUTTON_MARGIN +
              constants.V2_TEXT_GROUP_BUTTON_PADDING;
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
    if (this.v2Enabled) {
      const activeGroupId = this.getActiveGroupIdFromScrollPosition();

      const currentCategory =
          V2_SUBCATEGORY_TABS.find((tab) => tab.groupId === activeGroupId)
              ?.category;
      this.set('category', currentCategory);
    }
  }

  preventV2TabScrolling() {
    const tabs = this.getTabs();
    if (this.v2Enabled && tabs) {
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
  getHistoryEmojis(category: CategoryEnum) {
    if (this.incognito) {
      return [];
    }

    return this.categoriesHistory[category]?.data?.history?.map(
               emoji => ({
                 base: {string: emoji.base, name: emoji.name, keywords: []},
                 alternates: emoji.alternates.map(alternate => {
                   return {string: alternate, name: emoji.name, keywords: []};
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
    const historyIndex = V2_TABS_CATEGORY_START_INDEX.get(category);

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
  insertHistoryItem(category: CategoryEnum, item: {
    selectedEmoji: string,
    baseEmoji: string,
    alternates: string[],
    name: string,
  }) {
    if (this.incognito) {
      return;
    }

    const {selectedEmoji, baseEmoji, alternates, name} = item;

    this.categoriesHistory[category]?.bumpItem(
        {base: selectedEmoji, alternates: alternates, name: name});

    const preferenceUpdated =
        this.categoriesHistory[category]?.savePreferredVariant(
            baseEmoji, selectedEmoji);

    this.categoryHistoryUpdated(category, true, preferenceUpdated);
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
        Object.assign({}, baseDetails, V2_SUBCATEGORY_TABS[subcategoryIndex]));
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
        V2_SUBCATEGORY_TABS.filter(tab => tab.category === newCategoryName);
    this.set('emojiGroupTabs', categoryTabs);
    afterNextRender(this, () => {
      this.updateActiveGroup(true);
      this.updateHistoryTabDisabledProperty();
      const tabs = this.getTabs();
      if (tabs) {
        tabs.scrollLeft = this.calculateTabScrollLeftPosition(this.pagination);
      }
    });
  }

  onCategoryButtonClick(newCategoryName: string) {
    this.set('category', newCategoryName);
    this.set('pagination', 1);
    if (this.getSearchContainer()?.searchNotEmpty()) {
      this.getSearchContainer()?.setSearchQuery('');
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
  onPaginationChanged(newPage: number) {
    const tabs = this.getTabs();
    if (this.v2Enabled && tabs) {
      // Left chevron has the same margin as the text subcategory button.
      tabs.scrollLeft = this.calculateTabScrollLeftPosition(newPage);
    }
  }

  /**
   * Returns true if the subcategory bar requires text group buttons.
   */
  private isTextSubcategoryBarEnabled(v2Enabled: boolean, category: string) {
    // Categories that require its subcategory bar to be labelled by text.
    const textCategories = ['symbol', 'emoticon'];
    return v2Enabled && textCategories.includes(category);
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
   * Returns true if the page is not the first.
   */
  private isNotFirstPage(pageNumber: number) {
    return pageNumber !== 1;
  }

  getPaginationFromGroupId(groupId: string) {
    const tab = V2_SUBCATEGORY_TABS.find((tab) => tab.groupId === groupId);
    if (tab) {
      return tab.pagination;
    } else {
      throw new Error('Tab not found.');
    }
  }

  calculateTabScrollLeftPosition(page: number) {
    const chevronMargin = constants.V2_TAB_BUTTON_MARGIN;
    const offsetByLeftChevron = constants.V2_EMOJI_ICON_SIZE + chevronMargin;
    return (page === 1) ?
        0 :
        (page - 1) * constants.EMOJI_PICKER_WIDTH - offsetByLeftChevron;
  }

  getTabIndex(itemPagination: number, currentPagination: number) {
    return itemPagination === currentPagination ? 0 : -1;
  }

  getCategoryMetadata(category: string) {
    return CATEGORY_METADATA.map(data => ({
                                   name: data.name,
                                   icon: data.icon,
                                   active: data.name === category,
                                 }));
  }

  private getTabs() {
    return this.$['tabs'] as HTMLElement | null;
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
