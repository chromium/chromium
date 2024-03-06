// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://search-engine-choice/tangible_sync_style_shared.css.js';
import 'chrome://resources/cr_components/localized_link/localized_link.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.js';
import 'chrome://resources/cr_elements/cr_radio_button/cr_radio_button.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import './strings.m.js';

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import type {CrRadioGroupElement} from 'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {getFaviconForPageURL} from 'chrome://resources/js/icon.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {afterNextRender, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';
import type {SearchEngineChoice} from './browser_proxy.js';
import {SearchEngineChoiceBrowserProxy} from './browser_proxy.js';
import type {PageHandlerRemote} from './search_engine_choice.mojom-webui.js';

export interface SearchEngineChoiceAppElement {
  $: {
    infoDialog: CrDialogElement,
    actionButton: CrButtonElement,
    infoLink: HTMLElement,
    choiceList: CrRadioGroupElement,
  };
}

const SearchEngineChoiceAppElementBase = I18nMixin(PolymerElement);

export class SearchEngineChoiceAppElement extends
    SearchEngineChoiceAppElementBase {
  static get is() {
    return 'search-engine-choice-app';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The choice list is passed as JSON because it doesn't change
       * dynamically, so it would be better to have it available as loadtime
       * data.
       */
      choiceList_: {
        type: Array,
        value() {
          return JSON.parse(loadTimeData.getString('choiceList'));
        },
      },

      // The choice will always be > 0 when selected for prepopulated engines
      // and == 0 for a custom search engine.
      selectedChoice_: {
        type: Number,
        value: -1,
      },

      isActionButtonDisabled_: {
        type: Boolean,
        computed: 'computeActionButtonDisabled_(selectedChoice_, ' +
            'hasUserScrolledToTheBottom_)',
      },

      actionButtonText_: {
        type: String,
        computed: 'getActionButtonText_(hasUserScrolledToTheBottom_)',
      },

      hasUserScrolledToTheBottom_: {
        type: Boolean,
        value: false,
      },
    };
  }

  private choiceList_: SearchEngineChoice[];
  private selectedChoice_: string;
  private pageHandler_: PageHandlerRemote;
  private hasUserScrolledToTheBottom_: boolean;
  private actionButtonText_: string;

  constructor() {
    super();
    this.pageHandler_ = SearchEngineChoiceBrowserProxy.getInstance().handler;
  }

  override connectedCallback() {
    super.connectedCallback();

    // Change the `icon_path` format so that it can be used with the
    // `background-image` property in HTML. The
    // `background-image` property should be used because `getFaviconForPageURL`
    // returns an `image-set` and not a url.
    this.choiceList_.forEach((searchEngine: SearchEngineChoice) => {
      if (searchEngine.prepopulateId === 0) {
        // Fetch the favicon from the Favicon Service for custom search
        // engines.
        searchEngine.iconPath =
            getFaviconForPageURL(searchEngine?.url!, false, '', 24);
      } else {
        searchEngine.iconPath = 'image-set(url(' + searchEngine.iconPath +
            ') 1x, url(' + searchEngine.iconPath + '@2x) 2x)';
      }
    });

    afterNextRender(this, () => {
      // If the choice list and the page don't contain a scrollbar then the
      // user is already at the bottom.
      this.hasUserScrolledToTheBottom_ =
          !this.isChoiceListScrollable_() && !this.isPageScrollable_();

      if (this.isChoiceListScrollable_()) {
        this.$.choiceList.addEventListener(
            'scroll', this.onChoiceListScroll_.bind(this));
      }
      if (this.isPageScrollable_()) {
        document.addEventListener('scroll', this.onPageScroll_.bind(this));
      }

      this.pageHandler_.displayDialog();
    });
  }

  private onLinkClicked_() {
    this.$.infoDialog.showModal();
    this.pageHandler_.handleLearnMoreLinkClicked();
  }

  private needsUserChoice_() {
    return parseInt(this.selectedChoice_) === -1;
  }

  // The action button will be disabled if the user scrolls to the bottom of
  // the list without making a search engine choice.
  private computeActionButtonDisabled_() {
    return this.hasUserScrolledToTheBottom_ && this.needsUserChoice_();
  }

  private onActionButtonClicked_() {
    if (this.hasUserScrolledToTheBottom_) {
      this.pageHandler_.handleSearchEngineChoiceSelected(
          parseInt(this.selectedChoice_));
      return;
    }

    if (this.isChoiceListScrollable_()) {
      const choiceList = this.$.choiceList;
      choiceList.scrollTo({top: choiceList.scrollHeight, behavior: 'smooth'});
    } else if (this.isPageScrollable_()) {
      window.scrollTo({top: document.body.scrollHeight, behavior: 'smooth'});
    }
  }

  private onChevronClicked_(chevronExpanded: boolean) {
    if (chevronExpanded) {
      chrome.metricsPrivate.recordUserAction('ExpandSearchEngineDescription');
    }
  }

  private onInfoDialogButtonClicked_() {
    this.$.infoDialog.close();
  }

  private onChoiceListScroll_() {
    this.processScroll_(
        /*contentHeight=*/ this.$.choiceList.scrollHeight,
        /*viewportHeight=*/ this.$.choiceList.clientHeight,
        /*scrollPosition=*/ this.$.choiceList.scrollTop,
    );
  }

  private onPageScroll_() {
    this.processScroll_(
        /*contentHeight=*/ document.body.scrollHeight,
        /*viewportHeight=*/ window.innerHeight,
        /*scrollPosition=*/ window.scrollY,
    );
  }

  private processScroll_(
      contentHeight: number, viewportHeight: number, scrollPosition: number) {
    // The value is checked against `< 1` instead of `=== 0` to keep a margin of
    // error.
    if (contentHeight - viewportHeight - scrollPosition < 1) {
      this.hasUserScrolledToTheBottom_ = true;
      document.removeEventListener('scroll', this.onPageScroll_.bind(this));
      this.$.choiceList.removeEventListener(
          'scroll', this.onChoiceListScroll_.bind(this));
    }
  }

  // The choice list is scrollable at the dialog's full height.
  private isChoiceListScrollable_() {
    const choiceListOverflow = getComputedStyle(this.$.choiceList).overflow;
    return choiceListOverflow === 'auto' &&
        this.$.choiceList.scrollHeight > this.$.choiceList.clientHeight;
  }

  // The page becomes scrollable instead of the choice lists at specific
  // heights.
  private isPageScrollable_() {
    const choiceListOverflow = getComputedStyle(this.$.choiceList).overflow;
    return choiceListOverflow === 'visible' &&
        document.body.scrollHeight > document.body.clientHeight;
  }

  private getActionButtonText_() {
    return this.i18n(
        this.hasUserScrolledToTheBottom_ ? 'submitButtonText' :
                                           'moreButtonText');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'search-engine-choice-app': SearchEngineChoiceAppElement;
  }
}

customElements.define(
    SearchEngineChoiceAppElement.is, SearchEngineChoiceAppElement);
