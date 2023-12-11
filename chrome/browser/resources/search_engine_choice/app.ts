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
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import './strings.m.js';

import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {CrRadioGroupElement} from 'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {getFaviconForPageURL} from 'chrome://resources/js/icon.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {afterNextRender, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';
import {SearchEngineChoice, SearchEngineChoiceBrowserProxy} from './browser_proxy.js';
import {PageHandlerRemote} from './search_engine_choice.mojom-webui.js';

export interface SearchEngineChoiceAppElement {
  $: {
    dummyOmnibox: HTMLElement,
    infoDialog: CrDialogElement,
    searchEngineOmnibox: HTMLElement,
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
        observer: 'onSelectedChoiceChanged_',
      },

      isActionButtonDisabled_: {
        type: Boolean,
        computed: 'computeActionButtonDisabled_(selectedChoice_, ' +
            'hasUserScrolledToTheBottom_)',
      },

      fakeOmniboxText_: {
        type: String,
        value: '',
      },

      fakeOmniboxIconPath_: {
        type: String,
        value: '',
      },

      withMarketingSnippets_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('withMarketingSnippets');
        },
      },

      withForcedScroll_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('withForcedScroll');
        },
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
  private fakeOmniboxText_: string;
  private fakeOmniboxIconPath_: string;
  private pageHandler_: PageHandlerRemote;
  private withMarketingSnippets_: boolean;
  private hasUserScrolledToTheBottom_: boolean;
  private withForcedScroll_: boolean;
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
        searchEngine.iconPath = 'url(' + searchEngine.iconPath + ')';
      }
    });

    afterNextRender(this, () => {
      if (this.withForcedScroll_) {
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
      }

      this.pageHandler_.displayDialog();
    });
  }

  private onLinkClicked_() {
    this.$.infoDialog.showModal();
    this.pageHandler_.handleLearnMoreLinkClicked();
  }

  private needsScrollToTheBottom_() {
    return this.withForcedScroll_ && !this.hasUserScrolledToTheBottom_;
  }

  private needsUserChoice_() {
    return parseInt(this.selectedChoice_) === -1;
  }

  // The action button will be disabled if the user scrolls to the bottom of
  // the list without making a search engine choice.
  private computeActionButtonDisabled_() {
    return !this.needsScrollToTheBottom_() && this.needsUserChoice_();
  }

  private onActionButtonClicked_() {
    if (this.needsScrollToTheBottom_()) {
      if (this.isChoiceListScrollable_()) {
        const choiceList = this.$.choiceList;
        choiceList.scrollTo({top: choiceList.scrollHeight, behavior: 'smooth'});
      } else if (this.isPageScrollable_()) {
        window.scrollTo({top: document.body.scrollHeight, behavior: 'smooth'});
      }
      return;
    }
    this.pageHandler_.handleSearchEngineChoiceSelected(
        parseInt(this.selectedChoice_));
  }

  private onInfoDialogButtonClicked_() {
    this.$.infoDialog.close();
  }

  private onSelectedChoiceChanged_(selectedChoice: string) {
    // No search engine selected.
    if (parseInt(selectedChoice) === -1) {
      return;
    }

    // Get the selected engine.
    const choice = this.choiceList_.find(
        elem => elem.prepopulateId === parseInt(selectedChoice));
    const searchEngineOmnibox = this.$.searchEngineOmnibox;
    const dummyOmnibox = this.$.dummyOmnibox;
    const fakeOmniboxText = this.i18n('fakeOmniboxText', choice?.name!);
    const fakeOmniboxIconPath = choice?.iconPath!;

    // Change the previous engine name to the new one and then start
    // the fade-in-animation when the fade-out-animation finishes running.
    const handleFadeOutFinished = (event: AnimationEvent) => {
      if (event.animationName === 'fade-out-animation') {
        searchEngineOmnibox.classList.remove('fade-out-animation');

        this.fakeOmniboxText_ = fakeOmniboxText;
        this.fakeOmniboxIconPath_ = fakeOmniboxIconPath;
        // `requestAnimationFrame` is called to make sure that the previous
        // animation is fully removed so that the next one can be run.
        window.requestAnimationFrame(function() {
          searchEngineOmnibox.classList.add('fade-in-animation');
        });
      } else if (event.animationName === 'fade-in-animation') {
        // Hide the dummy omnibox so that it is not shown behind the
        // search engine omnibox.
        if (!dummyOmnibox.classList.contains('hidden')) {
          dummyOmnibox.classList.add('hidden');
        }
      }
    };

    // Show the dummy omnibox at fade-out start so that it can be seen while
    // animating the search engine omnibox.
    const handleAnimationStart = (event: AnimationEvent) => {
      if (event.animationName === 'fade-out-animation') {
        dummyOmnibox.classList.remove('hidden');
      }
    };
    searchEngineOmnibox.addEventListener('animationend', handleFadeOutFinished);
    searchEngineOmnibox.addEventListener(
        'animationstart', handleAnimationStart);

    if (searchEngineOmnibox.classList.contains('fade-in-animation')) {
      searchEngineOmnibox.classList.remove('fade-in-animation');

      window.requestAnimationFrame(function() {
        searchEngineOmnibox.classList.add('fade-out-animation');
      });
    } else {
      this.fakeOmniboxText_ = fakeOmniboxText;
      this.fakeOmniboxIconPath_ = fakeOmniboxIconPath;
      searchEngineOmnibox.classList.add('fade-in-animation');
    }
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
        this.needsScrollToTheBottom_() ? 'moreButtonText' : 'submitButtonText');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'search-engine-choice-app': SearchEngineChoiceAppElement;
  }
}

customElements.define(
    SearchEngineChoiceAppElement.is, SearchEngineChoiceAppElement);
