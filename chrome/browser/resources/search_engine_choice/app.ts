// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/localized_link/localized_link.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.js';
import 'chrome://resources/cr_elements/cr_radio_button/cr_radio_button.js';
import './strings.m.js';

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {getFaviconForPageURL} from 'chrome://resources/js/icon.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import type {SearchEngineChoice} from './browser_proxy.js';
import {SearchEngineChoiceBrowserProxy} from './browser_proxy.js';
import {PageHandler_ScrollState} from './search_engine_choice.mojom-webui.js';
import type {PageHandlerRemote} from './search_engine_choice.mojom-webui.js';

export interface AppElement {
  $: {
    actionButton: CrButtonElement,
    infoLink: HTMLElement,
    choiceList: HTMLElement,
    buttonContainer: HTMLElement,
  };
}

const AppElementBase = I18nMixinLit(CrLitElement);

export class AppElement extends AppElementBase {
  static get is() {
    return 'search-engine-choice-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      /**
       * The choice list is passed as JSON because it doesn't change
       * dynamically, so it would be better to have it available as loadtime
       * data.
       */
      choiceList_: {type: Array},

      // The choice will always be > 0 when selected for prepopulated engines
      // and == 0 for a custom search engine.
      selectedChoice_: {type: Number},

      isActionButtonDisabled_: {type: Boolean},
      actionButtonText_: {type: String},
      hasUserScrolledToTheBottom_: {type: Boolean},
      showInfoDialog_: {type: Boolean},
    };
  }

  protected choiceList_: SearchEngineChoice[] =
      JSON.parse(loadTimeData.getString('choiceList'));
  protected selectedChoice_: number = -1;
  protected isActionButtonDisabled_: boolean = false;
  protected hasUserScrolledToTheBottom_: boolean = false;
  protected showInfoDialog_: boolean = false;
  protected actionButtonText_: string = '';
  protected showGuestCheckbox_: boolean =
      loadTimeData.getBoolean('showGuestCheckbox');
  protected saveGuestModeSearchEngineChoice_: boolean = false;

  private resizeObserver_: ResizeObserver|null = null;
  private pageHandler_: PageHandlerRemote =
      SearchEngineChoiceBrowserProxy.getInstance().handler;

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
            getFaviconForPageURL(searchEngine.url!, false, '', 24);
      } else {
        searchEngine.iconPath = 'image-set(url(' + searchEngine.iconPath +
            ') 1x, url(' + searchEngine.iconPath + '@2x) 2x)';
      }
    });
    this.requestUpdate();

    this.addResizeObserver_();

    this.updateComplete.then(() => {
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

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedPrivateProperties.has('selectedChoice_')) {
      this.onSelectedChoiceChanged_(
          this.selectedChoice_,
          changedPrivateProperties.get('selectedChoice_') as number |
              undefined);
    }

    if (changedPrivateProperties.has('hasUserScrolledToTheBottom_')) {
      this.actionButtonText_ = this.i18n(
          this.hasUserScrolledToTheBottom_ ? 'submitButtonText' :
                                             'moreButtonText');
    }

    if (changedPrivateProperties.has('hasUserScrolledToTheBottom_') ||
        changedPrivateProperties.has('selectedChoice_')) {
      // The action button will be disabled if the user scrolls to the bottom of
      // the list without making a search engine choice.
      this.isActionButtonDisabled_ =
          this.hasUserScrolledToTheBottom_ && this.selectedChoice_ === -1;
    }
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

  protected onLinkClicked_(e: Event) {
    e.preventDefault();
    this.showInfoDialog_ = true;
    this.pageHandler_.handleLearnMoreLinkClicked();
  }

  protected onCheckboxStateChange_(e: CustomEvent<{value: boolean}>) {
    this.saveGuestModeSearchEngineChoice_ = e.detail.value;
  }

  protected onActionButtonClicked_() {
    if (this.hasUserScrolledToTheBottom_) {
      this.pageHandler_.handleSearchEngineChoiceSelected(
          this.selectedChoice_,
          this.showGuestCheckbox_ && this.saveGuestModeSearchEngineChoice_);
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

  protected getMarketingSnippetClass_(item: SearchEngineChoice) {
    return item.showMarketingSnippet ? '' : 'truncate-text';
  }

  protected onInfoDialogButtonClicked_() {
    this.showInfoDialog_ = false;
  }

  private resetSnippetState_(prepopulatedId: number) {
    if (prepopulatedId === -1) {
      return;
    }

    // Get the selected engine.
    const choice =
        this.choiceList_.find(elem => elem.prepopulateId === prepopulatedId)!;
    choice.showMarketingSnippet = false;
    this.requestUpdate();
  }

  private showSearchEngineSnippet_(prepopulateId: number) {
    if (prepopulateId === -1) {
      return;
    }

    // Get the selected engine.
    const choice =
        this.choiceList_.find(elem => elem.prepopulateId === prepopulateId)!;

    choice.showMarketingSnippet = true;
    this.requestUpdate();
  }

  protected onSelectedChoiceChangedByUser_(e: CustomEvent<{value: string}>) {
    this.selectedChoice_ = Number.parseInt(e.detail.value);
  }

  protected onSelectedChoiceChanged_(
      newPrepopulatedId: number, oldPrepopulatedId: number|undefined) {
    // No search engine selected.
    if (newPrepopulatedId === -1) {
      return;
    }

    chrome.metricsPrivate.recordUserAction('ExpandSearchEngineDescription');
    this.resetSnippetState_(oldPrepopulatedId as number);
    this.showSearchEngineSnippet_(newPrepopulatedId);
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
}

declare global {
  interface HTMLElementTagNameMap {
    'search-engine-choice-app': AppElement;
  }
}

customElements.define(AppElement.is, AppElement);
