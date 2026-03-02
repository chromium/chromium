// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_components/composebox/composebox_lens_search.js';
import '//resources/cr_components/composebox/contextual_entrypoint_button.js';
import '//resources/cr_components/composebox/recent_tab_chip.js';
import '//resources/cr_components/searchbox/searchbox_dropdown.js';
import '//resources/cr_elements/icons.html.js';
import '/strings.m.js';

import {ColorChangeUpdater} from '//resources/cr_components/color_change_listener/colors_css_updater.js';
import type {ContextualEntrypointButtonElement} from '//resources/cr_components/composebox/contextual_entrypoint_button.js';
import {SearchboxBrowserProxy} from '//resources/cr_components/searchbox/searchbox_browser_proxy.js';
import type {SearchboxDropdownElement} from '//resources/cr_components/searchbox/searchbox_dropdown.js';
import {kDefaultSelection} from '//resources/cr_components/searchbox/searchbox_match.js';
import {getInstance as getA11yAnnouncer} from '//resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {assertNotReached} from '//resources/js/assert.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {MetricsReporterImpl} from '//resources/js/metrics_reporter/metrics_reporter.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {SelectionDirection, SelectionLineState, SelectionStep} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {AutocompleteResult, OmniboxPopupSelection, PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerInterface as SearchboxPageHandlerInterface, TabInfo} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {InputState} from '//resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import {InputType} from '//resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import type {WindowOpenDisposition} from '//resources/mojo/ui/base/mojom/window_open_disposition.mojom-webui.js';
import type {Url} from '//resources/mojo/url/mojom/url.mojom-webui.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import type {PageCallbackRouter as OmniboxPopupPageCallbackRouter} from './omnibox_popup.mojom-webui.js';
import {OmniboxPopupBrowserProxy} from './omnibox_popup_browser_proxy.js';

// 675px ~= 449px (--cr-realbox-primary-side-min-width) * 1.5 + some margin.
const canShowSecondarySideMediaQueryList =
    window.matchMedia('(min-width: 675px)');

// Notifies an a11y announcer to read the aria-label for given element.
function announceElementAriaLabel(element: HTMLElement) {
  const message = element.getAttribute('aria-label');
  if (message) {
    // Note: ariaNotify is more efficient and appears to work more reliably, but
    // support is not guaranteed in all browsers. Fall back on a11y announcer if
    // the ariaNotify function is unavailable.
    const ariaNotify = (element as any).ariaNotify;
    if (ariaNotify) {
      ariaNotify.call(element, message);
    } else {
      getA11yAnnouncer(element)?.announce(message);
    }
  }
}

// Not all selection states of the webui popup are supported on the native
// browser side.
function selectionIsNativelySupported(s: OmniboxPopupSelection): boolean {
  return s.state !== SelectionLineState.kFocusedButtonContextEntrypoint;
}

function selectionsEqual(
    a: OmniboxPopupSelection, b: OmniboxPopupSelection): boolean {
  return a.line === b.line && a.state === b.state &&
      a.actionIndex === b.actionIndex;
}

function selectionToString(s: OmniboxPopupSelection) {
  return `{${s.line},${s.state},${s.actionIndex}}`;
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
      searchboxLayoutMode_: {reflect: true, type: String},
      showContextEntrypoint_: {type: Boolean},
      showAiModePrefEnabled_: {type: Boolean},
      isContentSharingEnabled_: {type: Boolean},
      isLensSearchEnabled_: {type: Boolean},
      isLensSearchEligible_: {type: Boolean},
      isAimEligible_: {type: Boolean},
      isAimButtonVisible_: {type: Boolean},
      isRecentTabChipEnabled_: {type: Boolean},
      recentTabForChip_: {type: Object},
      webuiOmniboxPopupSelectionControlEnabled_: {type: Boolean},
      inputState_: {type: Object},
      usePecApi_: {type: Boolean},
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
  protected accessor isContentSharingEnabled_: boolean = false;
  protected accessor isLensSearchEnabled_: boolean =
      loadTimeData.getBoolean('composeboxShowLensSearchChip');
  protected accessor isRecentTabChipEnabled_: boolean =
      loadTimeData.getBoolean('composeboxShowRecentTabChip');
  protected accessor webuiOmniboxPopupSelectionControlEnabled_: boolean =
      loadTimeData.getBoolean('webuiOmniboxPopupSelectionControlEnabled');
  protected accessor isLensSearchEligible_: boolean = false;
  protected accessor isAimEligible_: boolean = false;
  protected accessor isAimButtonVisible_: boolean = false;
  protected accessor recentTabForChip_: TabInfo|null = null;
  protected accessor inputState_: InputState|null = null;
  protected accessor usePecApi_: boolean =
      loadTimeData.getBoolean('contextualMenuUsePecApi');

  private callbackRouter_: SearchboxPageCallbackRouter;
  private eventTracker_ = new EventTracker();
  private listenerIds_: number[] = [];
  private pageHandler_: SearchboxPageHandlerInterface;
  private popupCallbackRouter_: OmniboxPopupPageCallbackRouter;
  private popupListenerIds_: number[] = [];
  private selection_: OmniboxPopupSelection = kDefaultSelection;

  constructor() {
    super();
    this.callbackRouter_ = SearchboxBrowserProxy.getInstance().callbackRouter;
    this.popupCallbackRouter_ =
        OmniboxPopupBrowserProxy.getInstance().callbackRouter;
    this.isDebug = new URLSearchParams(window.location.search).has('debug');
    this.pageHandler_ = SearchboxBrowserProxy.getInstance().handler;
    ColorChangeUpdater.forDocument().start();
  }

  override async connectedCallback() {
    super.connectedCallback();
    // TODO(b/468113419): The handlers and their definitions are not ordered the
    // same as the mojom file.
    this.popupListenerIds_ = [
      this.popupCallbackRouter_.onShow.addListener(this.onShow_.bind(this)),
    ];

    this.listenerIds_ = [
      this.callbackRouter_.autocompleteResultChanged.addListener(
          this.onAutocompleteResultChanged_.bind(this)),
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
      this.callbackRouter_.updateAimEligibility.addListener(
          (eligible: boolean) => {
            this.isAimEligible_ = eligible;
          }),
      this.callbackRouter_.onShowAiModePrefChanged.addListener(
          (canShow: boolean) => {
            this.showAiModePrefEnabled_ = canShow;
          }),
      this.callbackRouter_.updateContentSharingPolicy.addListener(
          (enabled: boolean) => {
            this.isContentSharingEnabled_ = enabled;
          }),
      this.callbackRouter_.onInputStateChanged.addListener(
          (inputState: InputState) => {
            this.inputState_ = inputState;
          }),
    ];
    if (this.webuiOmniboxPopupSelectionControlEnabled_) {
      this.listenerIds_.push(
          this.callbackRouter_.stepSelection.addListener(
              this.stepSelection_.bind(this)),
          this.callbackRouter_.openCurrentSelection.addListener(
              this.openCurrentSelection_.bind(this)),
          this.callbackRouter_.setAimButtonVisible.addListener(
              (visible: boolean) => {
                this.isAimButtonVisible_ = visible;
              }));
    }
    this.inputState_ = (await this.pageHandler_.getInputState()).state;
    canShowSecondarySideMediaQueryList.addEventListener(
        'change', this.onCanShowSecondarySideChanged_.bind(this));

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

    for (const listenerId of this.popupListenerIds_) {
      this.popupCallbackRouter_.removeListener(listenerId);
    }
    this.popupListenerIds_ = [];

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
        changedPrivateProperties.has('recentTabForChip_') ||
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
    return this.isAimEligible_ && this.showAiModePrefEnabled_ &&
        (isTallSearchbox || showContextualChipsInCompactMode) &&
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

    if (this.webuiOmniboxPopupSelectionControlEnabled_) {
      const available = this.getResultSelections_(this.result_);
      const sameLineSelection = {
        ...this.selection_,
        state: SelectionLineState.kNormal,
      };
      if (result.matches[0]?.allowedToBeDefaultMatch) {
        this.setSelection_(available[0] || kDefaultSelection);
      } else if (available.some(s => selectionsEqual(s, sameLineSelection))) {
        this.setSelection_(sameLineSelection);
      } else {
        this.setSelection_(kDefaultSelection);
      }
      return;
    }

    if (result.matches[0]?.allowedToBeDefaultMatch) {
      this.getDropdown().selectFirst();
    } else if (this.getDropdown().selectedMatchIndex >= result.matches.length) {
      this.getDropdown().unselect();
    }
  }

  private getContextualEntrypointButton_(): ContextualEntrypointButtonElement|
      null {
    if (this.showContextEntrypoint_ && !this.shouldHideEntrypointButton_) {
      return this.shadowRoot.querySelector<ContextualEntrypointButtonElement>(
          '#context');
    }
    return null;
  }

  private onShow_() {
    // When the popup is shown, blur the contextual entrypoint. This prevents a
    // focus ring from appearing on the entrypoint, e.g. when the user clicks
    // away and then re-focuses the Omnibox.
    this.getContextualEntrypointButton_()?.blur();
    this.refreshRecentTabForChip_();
  }

  protected onResultRepaint_() {
    const metricsReporter = MetricsReporterImpl.getInstance();
    metricsReporter.measure('ResultChanged')
        .then(
            duration => metricsReporter.umaReportTime(
                loadTimeData.getString('resultChangedToPaintMetricName'),
                duration))
        .then(() => metricsReporter.clearMark('ResultChanged'))
        // Ignore silently if mark 'ResultChanged' is missing.
        .catch(() => {});
  }

  private onUpdateSelection_(
      oldSelection: OmniboxPopupSelection, selection: OmniboxPopupSelection) {
    if (this.webuiOmniboxPopupSelectionControlEnabled_) {
      this.setSelection_(selection, false);
    } else {
      this.getDropdown().updateSelection(oldSelection, selection);
    }
  }

  private setSelection_(
      selection: OmniboxPopupSelection, notify: boolean = true) {
    const oldSelection = this.selection_;
    this.selection_ = selection;
    this.getDropdown().updateSelection(oldSelection, this.selection_);
    if (notify) {
      this.pageHandler_.setPopupSelection(
          selectionIsNativelySupported(this.selection_) ? this.selection_ :
                                                          kDefaultSelection);
    }

    const entrypoint = this.getContextualEntrypointButton_();
    if (entrypoint) {
      entrypoint.hasPopupFocus = this.selection_.state ===
          SelectionLineState.kFocusedButtonContextEntrypoint;
      if (entrypoint.hasPopupFocus) {
        announceElementAriaLabel(
            entrypoint.shadowRoot.querySelector('#entrypoint')!);
      }
    }
  }

  // Changes the current popup selection to the next selection in the order of
  // all available selections. That is, it translates a user intent (the given
  // `direction` and `step`) into a change of popup-focus (distinct from actual
  // browser/input focus, popup-focus shows what item from the popup will be
  // opened when user presses Enter).
  private stepSelection_(direction: SelectionDirection, step: SelectionStep) {
    this.setSelection_(
        this.getNextSelection_(this.selection_, direction, step));
  }

  // Given a current `from` selection, finds the next selection in the order
  // of all available selections. Traverses forward or backward by larger
  // or smaller steps as indicated by `direction` and `step` parameters,
  // which are in turn determined by user input signals such as arrow or
  // tab keystrokes.
  private getNextSelection_(
      from: OmniboxPopupSelection, direction: SelectionDirection,
      step: SelectionStep): OmniboxPopupSelection {
    if (!this.result_) {
      return from;
    }
    const available = this.getResultSelections_(this.result_);
    if (available.length === 0) {
      return from;
    }
    const isNormal = (selection: OmniboxPopupSelection) =>
        selection.state === SelectionLineState.kNormal;
    let fromIndex = available.findIndex(s => selectionsEqual(from, s));
    if (fromIndex < 0 && from.state === SelectionLineState.kKeywordMode) {
      // Second chance for keyword mode selections, to accommodate instant
      // keyword mode lines activating keyword mode from native side.
      fromIndex = available.findIndex(
          s =>
              selectionsEqual({...from, state: SelectionLineState.kNormal}, s));
    }
    if (fromIndex < 0) {
      available.splice(0, 0, from);
      fromIndex = 0;
    }
    if (step === SelectionStep.kAllLines) {
      const normalIndex = direction === SelectionDirection.kBackward ?
          available.findIndex(isNormal) :
          available.findLastIndex(isNormal);
      return normalIndex < 0 ? from : available[normalIndex]!;
    }
    for (let offset = 1; offset < available.length; offset++) {
      const index =
          (fromIndex +
           (direction === SelectionDirection.kForward ? offset : -offset) +
           available.length) %
          available.length;
      const selection = available[index]!;
      if (step === SelectionStep.kStateOrLine || isNormal(selection)) {
        return selection;
      }
    }
    return from;
  }

  // Returns the full set of selections available based on the given
  // AutocompleteResult. Note, this currently also depends on the
  // current popup state, e.g. the AI Mode button visibility, but
  // should eventually be driven entirely by a single data structure.
  private getResultSelections_(result: AutocompleteResult):
      OmniboxPopupSelection[] {
    const available = [];
    for (let matchIndex = 0; matchIndex < result.matches.length; matchIndex++) {
      const match = result.matches[matchIndex]!;
      if (match.isHidden) {
        continue;
      }
      available.push({
        line: matchIndex,
        state: SelectionLineState.kNormal,
        actionIndex: 0,
      });
      if (match.keywordChipHint.length > 0) {
        available.push({
          line: matchIndex,
          state: SelectionLineState.kKeywordMode,
          actionIndex: 0,
        });
      }
      for (let actionIndex = 0; actionIndex < match.actions.length;
           actionIndex++) {
        available.push({
          line: matchIndex,
          state: SelectionLineState.kFocusedButtonAction,
          actionIndex: actionIndex,
        });
      }
      if (match.supportsDeletion) {
        available.push({
          line: matchIndex,
          state: SelectionLineState.kFocusedButtonRemoveSuggestion,
          actionIndex: 0,
        });
      }
    }
    // TODO(crbug.com/462775253): Ideally everything available for selection
    // comes from the AutocompleteResult.
    if (this.showContextEntrypoint_ && !this.shouldHideEntrypointButton_) {
      available.push({
        line: -1,
        state: SelectionLineState.kFocusedButtonContextEntrypoint,
        actionIndex: 0,
      });
    }

    if (this.isAimButtonVisible_) {
      const insertionIndex =
          available.length > 0 && result.matches[0]?.allowedToBeDefaultMatch ?
          1 :
          0;
      available.splice(insertionIndex, 0, {
        // Use first default match if available (if not, the -1 means kNoMatch).
        line: result.matches.findIndex(m => m.allowedToBeDefaultMatch),
        state: SelectionLineState.kFocusedButtonAim,
        actionIndex: 0,
      });
      if (available.length === 1) {
        // If AIM button is the only selection available, provide a way to
        // deselect it.
        available.splice(0, 0, kDefaultSelection);
      }
    }
    return available;
  }

  // Opens the current popup selection (the one visually indicated by the
  // element with popup-focus).
  private openCurrentSelection_(disposition: WindowOpenDisposition) {
    if (this.selection_.state ===
        SelectionLineState.kFocusedButtonContextEntrypoint) {
      this.pageHandler_.showContextMenu({x: 0, y: 0});
    } else if (selectionIsNativelySupported(this.selection_)) {
      this.pageHandler_.openPopupSelection(this.selection_, disposition);
    } else {
      assertNotReached(
          `openCurrentSelection_ called for unsupported selection: ${
              selectionToString(this.selection_)}`);
    }
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

  protected async refreshRecentTabForChip_() {
    const {tabs} = await this.pageHandler_.getRecentTabs();
    this.recentTabForChip_ = tabs.find(tab => tab.showInCurrentTabChip) || null;
    if (!this.recentTabForChip_) {
      this.recentTabForChip_ =
          tabs.find(tab => tab.showInPreviousTabChip) || null;
    }
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
    // When "Always Show Full URL" is enabled the input has protocol etc.
    // so strip both input and url from the recent tab chip.
    const browserTabsAllowedByPecApi = !this.usePecApi_ ||
        (!!this.inputState_ &&
         this.inputState_.allowedInputTypes.includes(InputType.kBrowserTab));
    return this.isRecentTabChipEnabled_ && !!this.recentTabForChip_ &&
        browserTabsAllowedByPecApi &&
        (input?.length === 0 ||
         this.stripUrl_(input) === this.stripUrl_(this.recentTabForChip_?.url));
  }

  protected computeShowContextEntrypointDescription_(): boolean {
    const toolChipsVisible = this.isContentSharingEnabled_ &&
        (this.computeShowRecentTabChip_() || this.isLensSearchEligible_);
    return !toolChipsVisible;
  }

  private stripUrl_(url: string|undefined): string {
    if (!url) {
      return '';
    }
    return url.replace(/^https?:\/\/(?:www\.)?/, '').replace(/\/$/, '');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'omnibox-popup-app': OmniboxPopupAppElement;
  }
}

customElements.define(OmniboxPopupAppElement.is, OmniboxPopupAppElement);
