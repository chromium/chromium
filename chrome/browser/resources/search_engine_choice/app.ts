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
import './strings.m.js';

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {getFaviconForPageURL} from 'chrome://resources/js/icon.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {afterNextRender, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';
import type {SearchEngineChoice} from './browser_proxy.js';
import {SearchEngineChoiceBrowserProxy} from './browser_proxy.js';
import {PageHandler_ScrollState} from './search_engine_choice.mojom-webui.js';
import type {PageHandlerRemote} from './search_engine_choice.mojom-webui.js';

export interface SearchEngineChoiceAppElement {
  $: {
    infoDialog: CrDialogElement,
    actionButton: CrButtonElement,
    infoLink: HTMLElement,
    choiceList: HTMLElement,
    buttonContainer: HTMLElement,
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
        observer: 'onSelectedChoiceChanged_',
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

      snippetDisplayed_: Boolean,
    };
  }

  private choiceList_: SearchEngineChoice[];
  private selectedChoice_: string;
  private pageHandler_: PageHandlerRemote;
  private hasUserScrolledToTheBottom_: boolean;
  private actionButtonText_: string;
  private snippetDisplayed_: boolean;
  private resizeObserver_: ResizeObserver|null = null;

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

    this.addResizeObserver_();

    afterNextRender(this, () => {
      const isPageScrollable =
          document.body.scrollHeight > document.body.clientHeight;

      // If the page doesn't contain a scrollbar then the user is already at the
      // bottom.
      this.hasUserScrolledToTheBottom_ = !isPageScrollable;

      if (isPageScrollable) {
        document.addEventListener('scroll', this.onPageScroll_.bind(this));
      }

      window.addEventListener('resize', this.onPageResize_.bind(this));
      this.pageHandler_.displayDialog();
    });
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.resizeObserver_!.disconnect();
  }

  private addResizeObserver_() {
    function buttonAndListOverlap(
        buttonRect: DOMRect, listRect: DOMRect, offset: number): boolean {
      return !(
          buttonRect.right + offset < listRect.left ||
          buttonRect.left - offset > listRect.right ||
          buttonRect.bottom < listRect.top || buttonRect.top > listRect.bottom);
    }

    this.resizeObserver_ = new ResizeObserver(() => {
      // The button container should hide the remaining elements of the list
      // when they overlap so that the search engines and submit button don't
      // block each other.
      const buttonRect = this.$.actionButton.getBoundingClientRect();
      const listRect = this.$.choiceList.getBoundingClientRect();

      // We add an offset to mitigate the change in position caused by the
      // addition of the scrollbar.
      let offset = 0;
      if (this.$.choiceList.classList.contains('overlap-mitigation')) {
        offset = 30;
      }

      this.$.choiceList.classList.toggle(
          'overlap-mitigation',
          buttonAndListOverlap(buttonRect, listRect, offset));
      this.$.buttonContainer.classList.toggle(
          'overlap-mitigation',
          buttonAndListOverlap(buttonRect, listRect, offset));
    });
    this.resizeObserver_.observe(document.body);
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

    this.pageHandler_.handleMoreButtonClicked();
    document.addEventListener('scrollend', this.onPageScrollEnd_.bind(this));
    // Force change the "More" button to "Set as default" to prevent users from
    // being blocked on the screen in case of error.
    window.scrollTo({top: document.body.scrollHeight, behavior: 'smooth'});
  }

  private handleContentScrollStateUpdate_(forceFullyDisplayed: boolean) {
    if (!forceFullyDisplayed &&
        this.getScrollState_() === PageHandler_ScrollState.kNotAtTheBottom) {
      return;
    }

    this.hasUserScrolledToTheBottom_ = true;
    window.removeEventListener('resize', this.onPageResize_.bind(this));
    window.removeEventListener('scroll', this.onPageScroll_.bind(this));
    window.removeEventListener('scrollend', this.onPageScrollEnd_.bind(this));
  }

  private getMarketingSnippetClass_(item: SearchEngineChoice) {
    return item.showMarketingSnippet ? '' : 'truncate-text';
  }

  private onInfoDialogButtonClicked_() {
    this.$.infoDialog.close();
  }

  private resetSnippetState_(prepopulatedId: number) {
    if (prepopulatedId === -1) {
      return;
    }

    // Get the selected engine.
    const choice =
        this.choiceList_.find(elem => elem.prepopulateId === prepopulatedId)!;
    choice.showMarketingSnippet = false;
    this.snippetDisplayed_ = false;
  }

  private showSearchEngineSnippet_(prepopulateId: number) {
    if (prepopulateId === -1) {
      return;
    }

    // Get the selected engine.
    const choice =
        this.choiceList_.find(elem => elem.prepopulateId === prepopulateId)!;

    choice.showMarketingSnippet = true;
    this.snippetDisplayed_ = true;
  }

  private onSelectedChoiceChanged_(
      newPrepopulatedId: string, oldPrepopulatedId: string) {
    // No search engine selected.
    if (parseInt(newPrepopulatedId) === -1) {
      return;
    }

    chrome.metricsPrivate.recordUserAction('ExpandSearchEngineDescription');
    this.resetSnippetState_(parseInt(oldPrepopulatedId));
    this.showSearchEngineSnippet_(parseInt(newPrepopulatedId));
  }

  private onPageResize_() {
    this.handleContentScrollStateUpdate_(/*forceFullyDisplayed=*/ false);
  }

  private onPageScroll_() {
    this.handleContentScrollStateUpdate_(/*forceFullyDisplayed=*/ false);
  }

  private getScrollState_() {
    const scrollDifference =
        document.body.scrollHeight - window.innerHeight - window.scrollY;
    if (scrollDifference <= 0) {
      return PageHandler_ScrollState.kAtTheBottom;
    }
    return scrollDifference <= 1 ?
        PageHandler_ScrollState.kAtTheBottomWithErrorMargin :
        PageHandler_ScrollState.kNotAtTheBottom;
  }

  // This function is called only when the "More" button is clicked.
  private onPageScrollEnd_() {
    this.pageHandler_.recordScrollState(this.getScrollState_());
    this.handleContentScrollStateUpdate_(/*forceFullyDisplayed=*/ true);
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
