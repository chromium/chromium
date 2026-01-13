// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_components/composebox/contextual_entrypoint_and_carousel.js';
import '//resources/cr_components/searchbox/searchbox_dropdown.js';
import '//resources/cr_elements/icons.html.js';
import '/strings.m.js';

import {ColorChangeUpdater} from '//resources/cr_components/color_change_listener/colors_css_updater.js';
import type {ContextualEntrypointAndCarouselElement} from '//resources/cr_components/composebox/contextual_entrypoint_and_carousel.js';
import {SearchboxBrowserProxy} from '//resources/cr_components/searchbox/searchbox_browser_proxy.js';
import type {SearchboxDropdownElement} from '//resources/cr_components/searchbox/searchbox_dropdown.js';
import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {MetricsReporterImpl} from '//resources/js/metrics_reporter/metrics_reporter.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import type {AutocompleteResult, OmniboxPopupSelection, PageCallbackRouter, PageHandlerInterface, TabInfo} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {Url} from '//resources/mojo/url/mojom/url.mojom-webui.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';

// 675px ~= 449px (--cr-realbox-primary-side-min-width) * 1.5 + some margin.
const canShowSecondarySideMediaQueryList =
    window.matchMedia('(min-width: 675px)');

export interface OmniboxPopupAppElement {
  $: {
    context: ContextualEntrypointAndCarouselElement,
  };
}

// Displays the autocomplete matches in the autocomplete result.
export class OmniboxPopupAppElement extends I18nMixinLit
(CrLitElement) {
  static get is() {
    return 'omnibox-popup-app';
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
       * Whether the secondary side can be shown based on the feature state and
       * the width available to the dropdown.
       */
      canShowSecondarySide: {
        type: Boolean,
        reflect: true,
      },

      /*
       * Whether the secondary side is currently available to be shown.
       */
      hasSecondarySide: {
        type: Boolean,
        reflect: true,
      },

      /**
       * Whether the app is in debug mode.
       */
      isDebug: {
        type: Boolean,
        reflect: true,
      },

      /**
       * Whether matches are visible, as some may be hidden by filtering rules
       * (e.g., Gemini suggestions).
       */
      hasVisibleMatches_: {
        type: Boolean,
        reflect: true,
      },

      isInKeywordMode_: {type: Boolean},
      result_: {type: Object},
      searchboxLayoutMode_: {type: String},
      showContextEntrypoint_: {type: Boolean},
      showAiModePrefEnabled_: {type: Boolean},
      isLensSearchEnabled_: {type: Boolean},
      isLensSearchEligible_: {type: Boolean},
      isAimEligible_: {type: Boolean},
      isRecentTabChipEnabled_: {type: Boolean},
      tabSuggestions_: {type: Array},
    };
  }

  accessor canShowSecondarySide: boolean =
      canShowSecondarySideMediaQueryList.matches;
  accessor hasSecondarySide: boolean = false;
  accessor isDebug: boolean = false;
  protected accessor isInKeywordMode_: boolean = false;
  protected accessor showAiModePrefEnabled_: boolean = false;
  protected accessor hasVisibleMatches_: boolean = false;
  protected accessor result_: AutocompleteResult|null = null;
  protected accessor searchboxLayoutMode_: string =
      loadTimeData.getString('searchboxLayoutMode');
  protected accessor showContextEntrypoint_: boolean = false;
  protected accessor isLensSearchEnabled_: boolean =
      loadTimeData.getBoolean('composeboxShowLensSearchChip');
  protected accessor isRecentTabChipEnabled_: boolean =
      loadTimeData.getBoolean('composeboxShowRecentTabChip');
  protected accessor isLensSearchEligible_: boolean = false;
  protected accessor isAimEligible_: boolean = false;
  protected accessor tabSuggestions_: TabInfo[] = [];

  private callbackRouter_: PageCallbackRouter;
  private eventTracker_ = new EventTracker();
  private listenerIds_: number[] = [];
  private pageHandler_: PageHandlerInterface;

  constructor() {
    super();
    this.callbackRouter_ = SearchboxBrowserProxy.getInstance().callbackRouter;
    this.isDebug = new URLSearchParams(window.location.search).has('debug');
    this.pageHandler_ = SearchboxBrowserProxy.getInstance().handler;
    ColorChangeUpdater.forDocument().start();
  }

  override connectedCallback() {
    super.connectedCallback();
    // TODO(b:468113419): the handlers and their definitions are not ordered the
    // same as the
    //   mojom file.
    this.listenerIds_ = [
      this.callbackRouter_.autocompleteResultChanged.addListener(
          this.onAutocompleteResultChanged_.bind(this)),
      this.callbackRouter_.onShow.addListener(this.onShow_.bind(this)),
      this.callbackRouter_.updateSelection.addListener(
          this.onUpdateSelection_.bind(this)),
      this.callbackRouter_.setKeywordSelected.addListener(
          (isKeywordSelected: boolean) => {
            this.isInKeywordMode_ = isKeywordSelected;
          }),
      this.callbackRouter_.updateLensSearchEligibility.addListener(
          (eligible: boolean) => {
            this.isLensSearchEligible_ = this.isLensSearchEnabled_ && eligible;
          }),
      this.callbackRouter_.onTabStripChanged.addListener(
          this.refreshTabSuggestions_.bind(this)),
      this.callbackRouter_.updateAimEligibility.addListener(
          (eligible: boolean) => {
            this.isAimEligible_ = eligible;
          }),
      this.callbackRouter_.onShowAiModePrefChanged.addListener(
          (canShow: boolean) => {
            this.showAiModePrefEnabled_ = canShow;
          }),
    ];
    canShowSecondarySideMediaQueryList.addEventListener(
        'change', this.onCanShowSecondarySideChanged_.bind(this));

    this.refreshTabSuggestions_();

    if (!this.isDebug) {
      this.eventTracker_.add(
          document.documentElement, 'contextmenu', (e: Event) => {
            e.preventDefault();
          });
    }
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.eventTracker_.removeAll();
    for (const listenerId of this.listenerIds_) {
      this.callbackRouter_.removeListener(listenerId);
    }
    this.listenerIds_ = [];
    canShowSecondarySideMediaQueryList.removeEventListener(
        'change', this.onCanShowSecondarySideChanged_.bind(this));
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedPrivateProperties.has('result_')) {
      this.hasVisibleMatches_ =
          this.result_?.matches.some(match => !match.isHidden) ?? false;
    }

    if (changedPrivateProperties.has('isAimEligible_') ||
        changedPrivateProperties.has('searchboxLayoutMode_') ||
        changedPrivateProperties.has('isInKeywordMode_') ||
        changedPrivateProperties.has('showAiModePrefEnabled_') ||
        changedPrivateProperties.has('tabSuggestions_') ||
        changedPrivateProperties.has('result_') ||
        changedPrivateProperties.has('isLensSearchEligible_')) {
      this.showContextEntrypoint_ = this.computeShowContextEntrypoint_();
    }
  }

  getDropdown(): SearchboxDropdownElement {
    // Because there are 2 different cr-searchbox-dropdown instances that can be
    // exclusively shown, should always query the DOM to get the relevant one
    // and can't use this.$ to access it.
    return this.shadowRoot.querySelector('cr-searchbox-dropdown')!;
  }

  protected get shouldHideEntrypointButton_(): boolean {
    return this.searchboxLayoutMode_ === 'Compact';
  }

  private computeShowContextEntrypoint_(): boolean {
    const isTallSearchbox = this.searchboxLayoutMode_.startsWith('Tall');
    const showRecentTabChip = this.computeShowRecentTabChip_();
    const showContextualChips = showRecentTabChip || this.isLensSearchEligible_;
    const showContextualChipsInCompactMode =
        showContextualChips && this.searchboxLayoutMode_ === 'Compact';
    return this.isAimEligible_ && this.showAiModePrefEnabled_
        && (isTallSearchbox || showContextualChipsInCompactMode) &&
        !this.isInKeywordMode_;
  }

  private onCanShowSecondarySideChanged_(e: MediaQueryListEvent) {
    this.canShowSecondarySide = e.matches;
  }

  private onAutocompleteResultChanged_(result: AutocompleteResult) {
    // Skip empty results. Otherwise, blurring/closing the omnibox would clear
    // the results in the debug UI.
    if (this.isDebug && !result.matches.length) {
      return;
    }

    this.result_ = result;

    if (result.matches[0]?.allowedToBeDefaultMatch) {
      this.getDropdown().selectFirst();
    } else if (this.getDropdown().selectedMatchIndex >= result.matches.length) {
      this.getDropdown().unselect();
    }
  }

  private onShow_() {
    // When the popup is shown, blur the contextual entrypoint. This prevents a
    // focus ring from appearing on the entrypoint, e.g. when the user clicks
    // away and then re-focuses the Omnibox.
    if (this.showContextEntrypoint_) {
      this.$.context.blurEntrypoint();
    }
  }

  protected onResultRepaint_() {
    const metricsReporter = MetricsReporterImpl.getInstance();
    metricsReporter.measure('ResultChanged')
        .then(
            duration => metricsReporter.umaReportTime(
                'WebUIOmnibox.ResultChangedToRepaintLatency.ToPaint', duration))
        .then(() => metricsReporter.clearMark('ResultChanged'))
        // Ignore silently if mark 'ResultChanged' is missing.
        .catch(() => {});
  }

  private onUpdateSelection_(
      oldSelection: OmniboxPopupSelection, selection: OmniboxPopupSelection) {
    this.getDropdown().updateSelection(oldSelection, selection);
  }

  protected onHasSecondarySideChanged_(e: CustomEvent<{value: boolean}>) {
    this.hasSecondarySide = e.detail.value;
  }

  protected onContextualEntryPointClicked_(
      e: CustomEvent<{x: number, y: number}>) {
    e.preventDefault();
    const point = {
      x: e.detail.x,
      y: e.detail.y,
    };
    this.pageHandler_.showContextMenu(point);
  }

  protected async refreshTabSuggestions_() {
    const {tabs} = await this.pageHandler_.getRecentTabs();
    this.tabSuggestions_ = [...tabs];
  }

  protected onLensSearchChipClicked_() {
    this.pageHandler_.openLensSearch();
  }

  protected addTabContext_(e: CustomEvent<{
    id: number,
    title: string,
    url: Url,
    delayUpload: boolean,
  }>) {
    this.pageHandler_.addTabContext(e.detail.id, e.detail.delayUpload);
  }

  protected computeShowRecentTabChip_() {
    const input = this.result_?.input;
    let recentTabForChip =
        this.tabSuggestions_.find(tab => tab.showInCurrentTabChip) || null;
    if (!recentTabForChip) {
      recentTabForChip =
          this.tabSuggestions_.find(tab => tab.showInPreviousTabChip) || null;
    }
    return loadTimeData.getBoolean('composeboxShowRecentTabChip') &&
        (input?.length === 0 ||
         input ===
             recentTabForChip?.url.url.replace(/^https?:\/\/(?:www\.)?/, '')
                 .replace(/\/$/, ''));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'omnibox-popup-app': OmniboxPopupAppElement;
  }
}

customElements.define(OmniboxPopupAppElement.is, OmniboxPopupAppElement);
