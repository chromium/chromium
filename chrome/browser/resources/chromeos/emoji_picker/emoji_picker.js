// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './icons.html.js';
import './emoji_group.js';
import './emoji_group_button.js';
import './emoji_search.js';
import './text_group_button.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';

import {afterNextRender, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import * as constants from './constants.js';
import {EmojiGroupComponent} from './emoji_group.js';
import {getTemplate} from './emoji_picker.html.js';
import {Feature} from './emoji_picker.mojom-webui.js';
import {EmojiPickerApiProxy, EmojiPickerApiProxyImpl} from './emoji_picker_api_proxy.js';
import * as events from './events.js';
import {CATEGORY_METADATA, EMOJI_GROUP_TABS, V2_SUBCATEGORY_TABS, V2_TABS_CATEGORY_START_INDEX} from './metadata_extension.js';
import {RecentlyUsedStore} from './store.js';
import {CategoryData, CategoryEnum, EmojiGroup, EmojiGroupData, EmojiGroupElement, EmojiVariants, SubcategoryData} from './types.js';

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
      /** @private {CategoryEnum} */
      category: {type: String, value: 'emoji', observer: 'onCategoryChanged'},
      /** @type {string} */
      /** @private {!Array<!SubcategoryData>} */
      emojiGroupTabs: {type: Array},
      /** @type {EmojiGroupData} */
      categoriesData: {type: Array, value: () => ([])},
      /** @type {Array<EmojiGroupElement>} */
      categoriesGroupElements: {type: Array, value: () => ([])},
      /** @private {Object<CategoryEnum,RecentlyUsedStore>} */
      categoriesHistory: {type: Object, value: () => ({})},
      /** @private {number} */
      pagination: {type: Number, value: 1, observer: 'onPaginationChanged'},
      /** @private {boolean} */
      searchLazyIndexing: {type: Boolean, value: true},
      /** @private {boolean} */
      textSubcategoryBarEnabled: {
        type: Boolean,
        value: false,
        computed: 'isTextSubcategoryBarEnabled(v2Enabled, category)',
        reflectToAttribute: true,
      },
      /** @private {boolean} */
      v2Enabled: {type: Boolean, value: false, reflectToAttribute: true},
      /** @private {boolean} */
      searchExtensionEnabled: {type: Boolean, value: false},
      /** @private {boolean} */
      incognito: {type: Boolean, value: true},
    };
  }

  constructor() {
    super();

    // Incognito mode is set based on the default value.
    this.updateIncognitoState(this.incognito);

    this.emojiGroupTabs = EMOJI_GROUP_TABS;

    /** @private {?number} */
    this.scrollTimeout = null;

    /** @private {?number} */
    this.groupScrollTimeout = null;

    /** @private {?number} */
    this.groupButtonScrollTimeout = null;

    /** @private {?EmojiGroupComponent} */
    this.activeVariant = null;

    /** @private {!EmojiPickerApiProxy} */
    this.apiProxy_ = EmojiPickerApiProxyImpl.getInstance();

    /** @private {boolean} */
    this.autoScrollingToGroup = false;

    /** @private {boolean} */
    this.highlightBarMoving = false;

    /** @private {boolean} */
    this.groupTabsMoving = false;

    this.addEventListener(
        events.GROUP_BUTTON_CLICK, ev => this.selectGroup(ev.detail.group));
    this.addEventListener(
        events.EMOJI_BUTTON_CLICK, (ev) => this.onEmojiButtonClick(ev));
    this.addEventListener(
        events.EMOJI_CLEAR_RECENTS_CLICK, ev => this.clearRecentEmoji(ev));
    // variant popup related handlers
    this.addEventListener(
        events.EMOJI_VARIANTS_SHOWN,
        ev => this.onShowEmojiVariants(
            /** @type {!events.EmojiVariantsShownEvent} */ (ev)));
    this.addEventListener('click', () => this.hideDialogs());
    this.addEventListener(
      events.CATEGORY_BUTTON_CLICK,
      ev => this.onCategoryButtonClick(ev.detail.categoryName));
    this.addEventListener('search', ev => this.onSearchChanged(ev.detail));
  }

  /**
   * @private
   * @param {number} pageNumber
   */
  filterGroupTabByPagination(pageNumber) {
    return function(tab) {
      return tab.pagination === pageNumber && !tab.groupId.includes('history');
    };
  }

  initHistoryUI(incognito) {
    if (incognito !== this.incognito) {
      this.updateIncognitoState(incognito);
    }
    this.updateHistoryTabDisabledProperty();
    // Make highlight bar visible (now we know where it should be) and
    // add smooth sliding.
    this.updateActiveGroup(/*updateTabsScroll=*/ true);
    this.$.bar.style.display = 'block';
    this.$.bar.style.transition = 'left 200ms';
  }

  ready() {
    super.ready();

    // Ensure first category is emoji for compatibility with V1.
    if (CATEGORY_METADATA[0].name !== CategoryEnum.EMOJI) {
      throw new Error(
        `First category is ${CATEGORY_METADATA[0].name} but must be 'emoji'.`);
    }

    const dataUrls = EmojiPicker.configs().dataUrls;
    // Create an ordered list of category and urls based on the order that
    // categories need to appear in the UIs.
    const categoryDataUrls = CATEGORY_METADATA
        .filter(item => dataUrls[item.name])
        .map(item => ({'category': item.name, 'urls': dataUrls[item.name]}));

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
   *
   *
   * @param {Array<{category: CategoryEnum, urls: Array<string>}>}
   *    categoryDataUrls An array of categories and their corresponding data
   *    urls.
   */
  async fetchAndProcessData(categoryDataUrls) {
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

    // Update feature list, incognito state and fetch data of first url.
    const initialData = await Promise.all(
      [
        this.fetchOrderingData(dataUrls[0].url),
        this.apiProxy_.getFeatureList().then(
            (response) => this.setActiveFeatures(response.featureList)),
        this.apiProxy_.isIncognitoTextField().then(
            (response) => this.initHistoryUI(response.incognito)),
      ],
    ).then(values => values[0]); // Map to the fetched data only.

    if (this.v2Enabled) {
      this.updateStyles({
        '--emoji-size': constants.V2_EMOJI_ICON_SIZE_PX,
        '--emoji-spacing': constants.V2_EMOJI_SPACING_PX,
      });
    }

    // Update UI and relevant features based on the initial data.
    this.updateCategoryData(
      initialData, dataUrls[0].category,
      dataUrls[0].categoryLastPartition,
      !this.v2Enabled && dataUrls[0].categoryLastPartition);

    // Show the UI after the initial data is rendered.
    afterNextRender(this, () => {
      this.apiProxy_.showUI();
    });

    // Filter data urls based on the version. Remove the first url as it is
    // already added and shown.
    const remainingData = this.v2Enabled ?
      dataUrls.slice(1) :
      dataUrls.slice(1).filter(
        item => item.category === dataUrls[0].category);

    let prevFetchPromise = Promise.resolve();
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
  }

  /**
   * @param {!Array<!Feature>} featureList
   */
  setActiveFeatures(featureList) {
    this.v2Enabled = featureList.includes(Feature.EMOJI_PICKER_EXTENSION);
    this.searchExtensionEnabled =
        featureList.includes(Feature.EMOJI_PICKER_SEARCH_EXTENSION);
  }

  /**
   * @param {string} url
   */
  fetchOrderingData(url) {
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
  updateCategoryData(data, category, categoryLastPartition=false,
      lastPartition=false) {
    // TODO(b/233270589): Add category to the underlying data.
    // Add category field to the data.
    data.forEach((emojiGroup) => {
      emojiGroup.category = category;
    });

    // Create recently used emoji group for the category as its first
    // group element.
    if (V2_TABS_CATEGORY_START_INDEX[category] ===
        this.categoriesGroupElements.length) {
      const historyGroupElement = this.createEmojiGroupElement(
        this.getHistoryEmojis(category), {}, true,
        V2_TABS_CATEGORY_START_INDEX[category]);
      this.push('categoriesGroupElements', historyGroupElement);
    }

    // Convert the emoji group data to elements.
    const baseIndex = this.categoriesGroupElements.length;
    const categoriesGroupElements = [];

    data.filter(item => !item.searchOnly).forEach((emojiGroup, index) => {
      const tabIndex = baseIndex + index;
      const tabCategory = V2_SUBCATEGORY_TABS[tabIndex].category;
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
        V2_SUBCATEGORY_TABS[numEmojiGroups].category !== CategoryEnum.EMOJI;

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

  onSearchChanged(newValue) {
    this.$['list-container'].style.display = newValue ? 'none' : '';
  }

  onBarTransitionStart() {
    this.highlightBarMoving = true;
  }

  onBarTransitionEnd() {
    this.highlightBarMoving = false;
  }

  /**
   * @private
   * @param {Event} ev
   */
  onEmojiButtonClick(ev) {
    const category = ev.detail.category;
    delete ev.detail.category;
    this.insertText(category, ev.detail);
  }

  /**
   * @param {CategoryEnum} category
   * @param {{emoji: string, isVariant: boolean, baseEmoji: string,
   *  allVariants: ?Array<!string>, name: string}} item
   */
  async insertText(category, item) {
    const {text, isVariant, baseEmoji, allVariants, name} = item;
    this.$.message.textContent = text + ' inserted.';

    this.insertHistoryItem(category,
      {selectedEmoji: text, baseEmoji: baseEmoji,
        alternates: allVariants || [], name: name});

    const searchLength =
        /** @type {!CrSearchFieldElement} */ (
            this.$['search-container'].shadowRoot.querySelector(
                'cr-search-field'))
            .getSearchInput()
            .value.length;

    // TODO(b/217276960): change to a more generic name
    this.apiProxy_.insertEmoji(text, isVariant, searchLength);
  }

  clearRecentEmoji(event) {
    const category = event.detail.category;
    this.clearHistoryData(category);
    afterNextRender(
        this, () => {
          this.updateActiveGroup(/*updateTabsScroll=*/ true);
          this.updateHistoryTabDisabledProperty();
    });
  }

  /**
   * @param {string} newGroup
   */
  selectGroup(newGroup) {
    // focus and scroll to selected group's first emoji.
    const group =
        this.shadowRoot.querySelector(`div[data-group="${newGroup}"]`);

    if (group) {
      group.querySelector('.group')
          .shadowRoot.querySelector('#fake-focus-target')
          .focus();
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
      this.$.tabs.scrollLeft =
          constants.EMOJI_PICKER_TOTAL_EMOJI_WIDTH *
          constants.EMOJI_NUM_TABS_IN_FIRST_PAGE;
      this.scrollToGroup(
          EMOJI_GROUP_TABS[constants.GROUP_PER_ROW - 1].groupId);
      this.groupTabsMoving = true;
      this.$.bar.style.left = constants.EMOJI_PICKER_TOTAL_EMOJI_WIDTH_PX;
    } else {
      const maxPagination = this.getPaginationArray(this.emojiGroupTabs).pop();
      this.pagination = Math.min(this.pagination + 1, maxPagination);

      const nextTab =
          this.emojiGroupTabs.find((tab) => tab.pagination === this.pagination);
      this.scrollToGroup(nextTab.groupId);
      this.groupTabsMoving = true;
    }
  }

  onLeftChevronClick() {
    if (!this.v2Enabled) {
      this.$.tabs.scrollLeft = 0;
      this.scrollToGroup(EMOJI_GROUP_TABS[0].groupId);
      this.groupTabsMoving = true;
      if (!this.isCategoryHistoryEmpty(CategoryEnum.EMOJI)) {
        this.$.bar.style.left = '0';
      } else {
        this.$.bar.style.left = constants.EMOJI_PICKER_TOTAL_EMOJI_WIDTH_PX;
      }
    } else {
      this.pagination = Math.max(this.pagination - 1, 1);

      const nextTab =
          this.emojiGroupTabs.find((tab) => tab.pagination === this.pagination);
      this.scrollToGroup(nextTab.groupId);
      this.groupTabsMoving = true;
    }
  }

  /**
   * @param {string} newGroup The group ID to scroll to
   */
  scrollToGroup(newGroup) {
    // TODO(crbug/1152237): This should use behaviour:'smooth', but when you do
    // that it doesn't scroll.
    this.shadowRoot.querySelector(`div[data-group="${newGroup}"]`)
        .scrollIntoView();
  }

  /**
   * @private
   */
  onGroupsScroll() {
    this.updateChevrons();
    this.groupTabsMoving = true;

    if (this.groupButtonScrollTimeout) {
      clearTimeout(this.groupButtonScrollTimeout);
    }
    this.groupButtonScrollTimeout =
        setTimeout(this.groupTabScrollFinished.bind(this), 100);
  }

  /**
   * @private
   */
  groupTabScrollFinished() {
    this.groupTabsMoving = false;
    this.updateActiveGroup(/*updateTabsScroll=*/ false);
  }

  /**
   * @private
   */
  updateChevrons() {
    if (!this.v2Enabled) {
      if (this.$.tabs.scrollLeft > constants.GROUP_ICON_SIZE) {
        this.$['left-chevron'].style.display = 'flex';
      } else {
        this.$['left-chevron'].style.display = 'none';
      }
      // 1 less because we need to allow room for the chevrons
      if (this.$.tabs.scrollLeft + constants.GROUP_ICON_SIZE *
          constants.GROUP_PER_ROW <
          constants.GROUP_ICON_SIZE * (EMOJI_GROUP_TABS.length + 1)) {
        this.$['right-chevron'].style.display = 'flex';
      } else {
        this.$['right-chevron'].style.display = 'none';
      }
    } else if (this.v2Enabled && !this.textSubcategoryBarEnabled) {
      this.$['left-chevron'].style.display = 'none';
      this.$['right-chevron'].style.display = 'none';
    } else {
      this.$['left-chevron'].style.display =
          this.pagination >= 2 ? 'flex' : 'none';
      this.$['right-chevron'].style.display =
          this.pagination < this.getPaginationArray(this.emojiGroupTabs).pop() ?
          'flex' :
          'none';
    }
  }

  /**
   * @returns {string} the id of the emoji or emoticon group currently in view.
   */
  getActiveGroupIdFromScrollPosition() {
    // get bounding rect of scrollable emoji region.
    const thisRect = this.$.groups.getBoundingClientRect();

    const groupElements =
        Array.from(this.$.groups.querySelectorAll('[data-group]'));

    // activate the first group which is visible for at least 20 pixels,
    // i.e. whose bottom edge is at least 20px below the top edge of the
    // scrollable region.
    const activeGroup = groupElements.find(
        el => el.getBoundingClientRect().bottom - thisRect.top >= 20);

    const activeGroupId = activeGroup ?
        activeGroup.dataset.group : 'emoji-history';

    return activeGroupId;
  }

  /**
   * @param {boolean} updateTabsScroll
   */
  updateActiveGroup(updateTabsScroll) {
    const activeGroupId = this.getActiveGroupIdFromScrollPosition();
    this.set('pagination', this.getPaginationFromGroupId(activeGroupId));
    this.updateChevrons();

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
        this.$.bar.style.width = constants.EMOJI_HIGHLIGHTER_WIDTH_PX;

        // TODO(b/213120632): Convert the following number literals into
        // contextualized constants.
        let tabscrollLeft = this.$.tabs.scrollLeft;
        if (tabscrollLeft > constants.EMOJI_PICKER_TOTAL_EMOJI_WIDTH *
            (index - 0.5)) {
          tabscrollLeft = 0;
        }
        if (tabscrollLeft + constants.EMOJI_PICKER_TOTAL_EMOJI_WIDTH *
            (constants.GROUP_PER_ROW - 2) <
            constants.GROUP_ICON_SIZE * index) {
          // 5 = We want the seventh icon to be first. Then -1 for chevron, -1
          // for 1 based indexing.
          tabscrollLeft = constants.EMOJI_PICKER_TOTAL_EMOJI_WIDTH *
              (constants.EMOJI_NUM_TABS_IN_FIRST_PAGE - 1);
        }

        if (updateTabsScroll) {
          this.$.bar.style.left =
              (index * constants.EMOJI_PICKER_TOTAL_EMOJI_WIDTH -
              tabscrollLeft) + 'px';
        } else {
          this.$.bar.style.left =
              (index * constants.EMOJI_PICKER_TOTAL_EMOJI_WIDTH -
              this.$.tabs.scrollLeft) + 'px';
        }

        // only update tab scroll when using emoji-based subcategory, because
        // the scroll position of the text-based subcategory bar is controlled
        // differently.
        if (updateTabsScroll && !this.textSubcategoryBarEnabled) {
          this.$.tabs.scrollLeft = tabscrollLeft;
        }
      } else if (this.v2Enabled && !this.textSubcategoryBarEnabled) {
        this.$.bar.style.width = constants.EMOJI_HIGHLIGHTER_WIDTH_PX;
        this.$.bar.style.left =
            `${index * constants.V2_EMOJI_PICKER_TOTAL_EMOJI_WIDTH}px`;
      } else {
        const subcategoryTabs =
            Array.from(this.$.tabs.getElementsByClassName('tab'));

        // for text group button, the highlight bar only spans its inner width,
        // which excludes both padding and margin.
        if (index < subcategoryTabs.length) {
          const barInlineGap =
              constants.V2_TAB_BUTTON_MARGIN +
              constants.V2_TEXT_GROUP_BUTTON_PADDING;
          const currentTab = subcategoryTabs[index];
          this.$.bar.style.left = `${
              currentTab.offsetLeft - constants.EMOJI_PICKER_SIDE_PADDING -
              this.calculateTabScrollLeftPosition(this.pagination)}px`;
          this.$.bar.style.width =
              `${subcategoryTabs[index].clientWidth - barInlineGap * 2}px`;
        } else {
          this.$.bar.style.left = `0px`;
          this.$.bar.style.width = `0px`;
        }
        // TODO(b/213230435): fix the bar width and left position when the
        // history tab is active
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
              .category;
      this.set('category', currentCategory);
    }
  }

  preventV2TabScrolling() {
    if (this.v2Enabled) {
      this.$.tabs.scrollLeft =
          this.calculateTabScrollLeftPosition(this.pagination);
    }
  }

  hideDialogs() {
    this.hideEmojiVariants();

    for (const category of Object.values(CategoryEnum)) {
      if (!this.isCategoryHistoryEmpty(category)) {
        const historyButton = this.shadowRoot.querySelector(
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
   *
   * @param {CategoryEnum} category Category of the history.
   * @return {!Array<EmojiVariants>} List of emojis.
   */
  getHistoryEmojis(category) {
    if (this.incognito) {
      return [];
    }

    return this.categoriesHistory[category].data.history.map(
        emoji => ({
          base: {string: emoji.base, name: emoji.name, keywords: []},
          alternates: emoji.alternates,
        }));
  }

  /**
   * Handles the event where history or preferences are modified for a
   * category.
   *
   * @param {CategoryEnum} category Category of the modified history.
   * @param {boolean} historyUpdated True only if history items are updated.
   * @param {boolean} preferenceUpdated True only if preferences are updated.
   */
  categoryHistoryUpdated(category,
      historyUpdated = true, preferenceUpdated = true) {

    // History item is assumed to be the first item of each category.
    const historyIndex = V2_TABS_CATEGORY_START_INDEX[category];

    // If history group is already added, then update it.
    if (historyUpdated &&
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
   * @param {boolean} incognito True for incognito mode.
   */
  updateIncognitoState(incognito) {
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
   * @param {CategoryEnum} category
   * @param {{selectedEmoji: string, baseEmoji: string,
   *         alternates: !Array<!string>, name: string}} item
   */
  insertHistoryItem(category, item) {
    if (this.incognito) {
      return;
    }

    const {selectedEmoji, baseEmoji, alternates, name} = item;

    this.categoriesHistory[category].bumpItem({
      base: selectedEmoji, alternates: alternates, name: name});

    const preferenceUpdated = this.categoriesHistory[category]
      .savePreferredVariant(baseEmoji, selectedEmoji);

    this.categoryHistoryUpdated(category, true, preferenceUpdated);
  }

  /**
   * Clears history items for a category.
   *
   * @param {CategoryEnum} category Category of the history items.
   */
  clearHistoryData(category) {
    if (this.incognito) {
      return;
    }

    this.categoriesHistory[category].clearRecents();
    this.categoryHistoryUpdated(category, true, false);
  }

  /**
   * Check if the history items of a category is empty.
   *
   * @param {CategoryEnum} category Input category.
   * @returns {boolean} True for empty history.
   */
  isCategoryHistoryEmpty(category) {
    return this.incognito ||
        this.categoriesHistory[category].data.history.length == 0;
  }

  /**
   * Gets HTML classes for an emoji group element.
   *
   * @param {boolean} isHistory If group is history.
   * @param {Array<EmojiVariants>} emojis List of emojis
   * @returns {string} HTML element class attribute.
   */
  getEmojiGroupClassNames(isHistory, emojis) {
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
   * @param {Array<EmojiVariants>} emoji List of emojis.
   * @param {Object<string,string>} preferences Preferences for emojis.
   * @param {boolean} isHistory True if group is for history.
   * @param {!number} subcategoryIndex Index of the group in subcategory data.
   * @returns {EmojiGroupElement} Instance of emoji group element.
   */
  createEmojiGroupElement(emoji, preferences, isHistory, subcategoryIndex) {
    const baseDetails = {
      'emoji': emoji,
      'preferences': preferences,
      'isHistory': isHistory,
    };
    return /** @type {EmojiGroupElement} */ (
      Object.assign(
        {}, baseDetails, V2_SUBCATEGORY_TABS[subcategoryIndex]));
  }

  /**
   * Gets preferences for an emoji group.
   *
   * @param {CategoryEnum} category Category of the emoji group.
   * @returns {Object<string,string>} Preferences.
   */
  getEmojiGroupPreference(category) {
    return this.incognito ? [] :
        this.categoriesHistory[category].data.preference;
  }

  /**
   * @param {!events.EmojiVariantsShownEvent} ev
   */
  onShowEmojiVariants(ev) {

    // Hide the currently shown emoji variants if the new one belongs
    // to a different emoji group.
    if (this.activeVariant && ev.detail.owner !== this.activeVariant) {
      this.hideEmojiVariants();
    }

    this.activeVariant =
        /** @type {EmojiGroupComponent} */ (ev.detail.owner);

    // Updates the UI if a variant is shown.
    if (ev.detail.variants) {
      this.$.message.textContent = ev.detail.baseEmoji + ' variants shown.';
      this.positionEmojiVariants(ev.detail.variants);
    }
  }

  positionEmojiVariants(variants) {
    // TODO(crbug.com/1174311): currently positions horizontally within page.
    // ideal UI would be overflowing the bounds of the page.
    // also need to account for vertical positioning.

    // compute width required for the variant popup as: SIZE * columns + 10.
    // SIZE is emoji width in pixels. number of columns is determined by width
    // of variantRows, then one column each for the base emoji and skin tone
    // indicators if present. 10 pixels are added for padding and the shadow.

    // Reset any existing left margin before calculating a new position.
    variants.style.marginLeft = 0;

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
    if (variantBottom > scrollTop + groups.offsetHeight + listTop) {
      groups.scrollTo({
        top: variantBottom - groups.offsetHeight - listTop,
        left: 0,
        behavior: 'smooth',
      });
    }
  }

  /**
   * Triggers when category property changes
   * @param {string} newCategoryName
   */
  onCategoryChanged(newCategoryName) {
    const categoryTabs =
        V2_SUBCATEGORY_TABS.filter(tab => tab.category === newCategoryName);
    this.set('emojiGroupTabs', categoryTabs);
    afterNextRender(this, () => {
      this.updateActiveGroup(true);
      this.updateHistoryTabDisabledProperty();
      this.$.tabs.scrollLeft =
          this.calculateTabScrollLeftPosition(this.pagination);
    });
  }

  /**
   * @param {string} newCategoryName
   */
  onCategoryButtonClick(newCategoryName) {
    this.set('category', newCategoryName);
    this.set('pagination', 1);
    if (this.$['search-container'].searchNotEmpty()) {
      this.$['search-container'].setSearchQuery('');
      afterNextRender(this, () => {
        this.scrollToGroup(this.emojiGroupTabs[0].groupId);
      });
    } else {
      this.scrollToGroup(this.emojiGroupTabs[0].groupId);
    }
  }

  /**
   * Trigger when pagination changes
   * @param {number} newPage
   */
  onPaginationChanged(newPage) {
    if (this.v2Enabled) {
      // Left chevron has the same margin as the text subcategory button.
      this.$.tabs.scrollLeft = this.calculateTabScrollLeftPosition(newPage);
    }
  }

  /**
   * Returns true if the subcategory bar requires text group buttons.
   * @private
   * @param {boolean} v2Enabled
   * @param {string} category
   */
  isTextSubcategoryBarEnabled(v2Enabled, category) {
    // Categories that require its subcategory bar to be labelled by text.
    const textCategories = ['symbol', 'emoticon'];
    return v2Enabled && textCategories.includes(category);
  }

  /**
   * Returns the array of page numbers which starts at 1 and finishes at the
   * last pagination.
   * @private
   * @param {Array<SubcategoryData>} tabs
   */
  getPaginationArray(tabs) {
    const paginations = tabs.map(tab => tab.pagination).filter(num => num);
    const lastPagination = Math.max(...paginations);
    return Array.from(Array(lastPagination), (_, idx) => idx + 1);
  }

  /**
   * Returns true if the page is not the first.
   * @private
   * @param {number} pageNumber
   */
  isNotFirstPage(pageNumber) {
    return pageNumber !== 1;
  }

  /**
   * @param {string} groupId
   * @returns {number}
   * @throws Thrown when no tab with id that matches the given groupId is found.
   */
  getPaginationFromGroupId(groupId) {
    const tab = V2_SUBCATEGORY_TABS.find((tab) => tab.groupId === groupId);
    if (tab) {
      return tab.pagination;
    } else {
      throw new Error('Tab not found.');
    }
  }

  /**
   * @param {number} page
   * @returns {number}
   */
  calculateTabScrollLeftPosition(page) {
    const chevronMargin = constants.V2_TAB_BUTTON_MARGIN;
    const offsetByLeftChevron = constants.V2_EMOJI_ICON_SIZE + chevronMargin;
    return (page === 1) ? 0 :
        (page - 1) * constants.EMOJI_PICKER_WIDTH - offsetByLeftChevron;
  }

  /**
   * @param {number} itemPagination
   * @param {number} currentPagination
   * @returns {number}
   */
  getTabIndex(itemPagination, currentPagination) {
    return itemPagination === currentPagination ? 0 : -1;
  }

  /**
   * @private
   * @param {string} category
   * @returns {!Array<!CategoryData>}
   */
  getCategoryMetadata(category) {
    return CATEGORY_METADATA.map(data => ({
                                   name: data.name,
                                   icon: data.icon,
                                   active: data.name === category,
                                 }));
  }
}

customElements.define(EmojiPicker.is, EmojiPicker);
