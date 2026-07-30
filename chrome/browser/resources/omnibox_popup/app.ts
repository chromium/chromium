// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_components/composebox/composebox_lens_search.js';
import '//resources/cr_components/composebox/contextual_entrypoint_button.js';
import '//resources/cr_components/composebox/current_tab_chip.js';
import '//resources/cr_components/searchbox/searchbox_dropdown.js';
import '//resources/cr_elements/icons.html.js';
import '/strings.m.js';

import {ColorChangeUpdater} from '//resources/cr_components/color_change_listener/colors_css_updater.js';
import type {ContextualEntrypointButtonElement} from '//resources/cr_components/composebox/contextual_entrypoint_button.js';
import {SearchboxBrowserProxy} from '//resources/cr_components/searchbox/searchbox_browser_proxy.js';
import type {SearchboxDropdownElement} from '//resources/cr_components/searchbox/searchbox_dropdown.js';
import {kDefaultSelection} from '//resources/cr_components/searchbox/searchbox_match.js';
import {SearchboxSelectionMixin, selectionIsNativelySupported, selectionsEqual, selectionToString} from '//resources/cr_components/searchbox/searchbox_selection_mixin.js';
import type {AutocompleteResult, OmniboxPopupSelection, SelectionDirection, SelectionStep} from '//resources/cr_components/searchbox/searchbox_selection_mixin.js';
import {SelectionLineState} from '//resources/cr_components/searchbox/searchbox_selection_mixin.js';
import {getInstance as getA11yAnnouncer} from '//resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {assertNotReached} from '//resources/js/assert.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {MetricsReporterImpl} from '//resources/js/metrics_reporter/metrics_reporter.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import type {TabInfo} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {InputType} from '//resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import type {InputState} from '//resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import type {WindowOpenDisposition} from '//resources/mojo/ui/base/mojom/window_open_disposition.mojom-webui.js';
import type {Url} from '//resources/mojo/url/mojom/url.mojom-webui.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import type {BrowserProxy} from './omnibox_popup.mojom-webui.js';
import {browserProxyFactory} from './omnibox_popup.mojom-webui.js';

// 675px ~= 449px (--cr-realbox-primary-side-min-width) * 1.5 + some margin.
const canShowSecondarySideMediaQueryList =
    window.matchMedia('(min-width: 675px)');

// Displays the autocomplete matches in the autocomplete result.
export class OmniboxPopupAppElement extends SearchboxSelectionMixin
(I18nMixinLit(CrLitElement)) {
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

      result_: {type: Object},
      searchboxLayoutMode_: {reflect: true, type: String},
      showContextEntrypoint_: {
        type: Boolean,
        reflect: true,
      },
      showContextButtonSuggestionLabel_: {type: Boolean},
      isContentSharingEnabled_: {type: Boolean},
      isLensSearchEnabled_: {type: Boolean},
      isLensSearchEligible_: {type: Boolean},
      isAimPopupEligible_: {type: Boolean},
      isLensChipShown_: {type: Boolean},
      isAimButtonVisible_: {type: Boolean},
      isCurrentTabChipEnabled_: {type: Boolean},
      isLensIconEnabled_: {type: Boolean},
      isLensIconEligible_: {type: Boolean},
      isLensIconShown_: {type: Boolean},
      currentTabForChip_: {type: Object},
      isCurrentTabChipShown_: {type: Boolean},
      webuiOmniboxPopupSelectionControlEnabled_: {type: Boolean},
      inputState_: {type: Object},
      usePecApi_: {type: Boolean},
      applyContextButtonBackground_: {type: Boolean},
      isOblongShape_: {type: Boolean},
    };
  }

  accessor canShowSecondarySide: boolean =
      canShowSecondarySideMediaQueryList.matches;
  accessor hasSecondarySide: boolean = false;
  accessor isDebug: boolean = false;
  protected accessor hasVisibleMatches_: boolean = false;
  protected accessor result_: AutocompleteResult|null = null;
  protected accessor searchboxLayoutMode_: string =
      loadTimeData.getString('searchboxLayoutMode');
  protected accessor showContextEntrypoint_: boolean = false;
  protected accessor isContentSharingEnabled_: boolean = false;
  protected accessor isLensSearchEnabled_: boolean =
      loadTimeData.getBoolean('composeboxShowLensSearchChip');
  protected accessor webuiOmniboxPopupSelectionControlEnabled_: boolean =
      loadTimeData.getBoolean('webuiOmniboxPopupSelectionControlEnabled');
  protected accessor isLensSearchEligible_: boolean = false;
  protected accessor isLensChipShown_: boolean = false;
  protected accessor isAimPopupEligible_: boolean = false;
  protected accessor isAimButtonVisible_: boolean = false;
  protected accessor isCurrentTabChipEnabled_: boolean =
      loadTimeData.getBoolean('composeboxShowCurrentTabChip');
  protected accessor isLensIconEnabled_: boolean =
      loadTimeData.getBoolean('composeboxShowLensIcon');
  protected accessor isLensIconEligible_: boolean = false;
  protected accessor isLensIconShown_: boolean = false;
  protected accessor currentTabForChip_: TabInfo|null = null;
  protected accessor isCurrentTabChipShown_: boolean = false;
  protected accessor inputState_: InputState|null = null;
  protected accessor usePecApi_: boolean =
      loadTimeData.getBoolean('contextualMenuUsePecApi');
  protected accessor applyContextButtonBackground_: boolean = false;
  protected accessor isOblongShape_: boolean =
      loadTimeData.getBoolean('contextButtonShapeIsOblong');

  override get isAimButtonVisible(): boolean {
    return this.isAimButtonVisible_;
  }

  override get showContextEntrypoint(): boolean {
    return this.showContextEntrypoint_ && !this.shouldHideEntrypointButton_();
  }

  private searchboxBrowserProxy_: SearchboxBrowserProxy;
  private eventTracker_ = new EventTracker();
  private hideContextButton_: boolean =
      loadTimeData.getBoolean('hideClassicContextButton');
  private contextButtonHasBackground_: boolean =
      loadTimeData.getBoolean('contextButtonHasBackground');
  protected accessor showContextButtonSuggestionLabel_: boolean =
      loadTimeData.getBoolean('omniboxShowContextButtonSuggestionLabel');
  private listenerIds_: number[] = [];

  private browserProxy_: BrowserProxy;
  private popupListenerIds_: number[] = [];

  constructor() {
    super();
    this.searchboxBrowserProxy_ = SearchboxBrowserProxy.getInstance();
    this.browserProxy_ = browserProxyFactory.getInstance();
    this.isDebug = new URLSearchParams(window.location.search).has('debug');
    ColorChangeUpdater.forDocument().start();
  }

  override async connectedCallback() {
    super.connectedCallback();
    // TODO(b/468113419): The handlers and their definitions are not ordered the
    // same as the mojom file.
    this.popupListenerIds_ = [
      this.browserProxy_.callbackRouter.onShow.addListener(
          this.onShow_.bind(this)),
      this.browserProxy_.callbackRouter.onContextMenuClosed.addListener(
          this.onContextMenuClosed_.bind(this)),

    ];

    this.listenerIds_ = [
      this.searchboxBrowserProxy_.callbackRouter.autocompleteResultChanged
          .addListener(this.onAutocompleteResultChanged_.bind(this)),
      this.searchboxBrowserProxy_.callbackRouter.updateSelection.addListener(
          this.onUpdateSelection_.bind(this)),
      this.searchboxBrowserProxy_.callbackRouter.updateLensSearchEligibility
          .addListener((eligible: boolean) => {
            this.isLensSearchEligible_ = this.isLensSearchEnabled_ && eligible;
            this.isLensIconEligible_ = this.isLensIconEnabled_ && eligible;
          }),
      this.searchboxBrowserProxy_.callbackRouter.updateContentSharingPolicy
          .addListener((enabled: boolean) => {
            this.isContentSharingEnabled_ = enabled;
          }),
      this.searchboxBrowserProxy_.callbackRouter.onInputStateChanged
          .addListener((inputState: InputState) => {
            this.inputState_ = inputState;
          }),
    ];
    if (!this.hideContextButton_) {
      this.listenerIds_.push(
          this.searchboxBrowserProxy_.callbackRouter.updateAimPopupEligibility
              .addListener((eligible: boolean) => {
                this.isAimPopupEligible_ = eligible;
              }));
    }
    if (this.webuiOmniboxPopupSelectionControlEnabled_) {
      this.listenerIds_.push(
          this.searchboxBrowserProxy_.callbackRouter.stepSelection.addListener(
              this.stepSelection_.bind(this)),
          this.searchboxBrowserProxy_.callbackRouter.openCurrentSelection
              .addListener(this.openCurrentSelection_.bind(this)),
          this.searchboxBrowserProxy_.callbackRouter.setAimButtonVisible
              .addListener((visible: boolean) => {
                this.isAimButtonVisible_ = visible;
              }));
    }
    this.inputState_ =
        (await this.searchboxBrowserProxy_.handler.getInputState()).state;
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
      this.searchboxBrowserProxy_.callbackRouter.removeListener(listenerId);
    }
    this.listenerIds_ = [];

    for (const listenerId of this.popupListenerIds_) {
      this.browserProxy_.callbackRouter.removeListener(listenerId);
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

    if (changedPrivateProperties.has('isAimPopupEligible_') ||
        changedPrivateProperties.has('searchboxLayoutMode_') ||
        changedPrivateProperties.has('result_') ||
        changedPrivateProperties.has('isLensSearchEligible_')) {
      this.showContextEntrypoint_ = this.computeShowContextEntrypoint_();
    }

    if (changedPrivateProperties.has('isContentSharingEnabled_') ||
        changedPrivateProperties.has('isLensSearchEligible_') ||
        changedPrivateProperties.has('isLensIconEligible_') ||
        changedPrivateProperties.has('currentTabForChip_') ||
        changedPrivateProperties.has('inputState_')) {
      this.isCurrentTabChipShown_ = this.isContentSharingEnabled_ &&
          this.isLensSearchEligible_ && this.computeShowCurrentTabChip_();
      this.isLensIconShown_ = this.isContentSharingEnabled_ &&
          this.isLensIconEligible_;
      this.isLensChipShown_ = this.isContentSharingEnabled_ &&
          this.isLensSearchEligible_ && !this.isCurrentTabChipShown_;
      this.applyContextButtonBackground_ =
          this.contextButtonHasBackground_ && !this.isLensChipShown_;
    }
  }

  getDropdown(): SearchboxDropdownElement {
    // Because there are 2 different cr-searchbox-dropdown instances that can be
    // exclusively shown, should always query the DOM to get the relevant one
    // and can't use this.$ to access it.
    return this.shadowRoot.querySelector('cr-searchbox-dropdown')!;
  }

  protected shouldHideEntrypointButton_(): boolean {
    return this.searchboxLayoutMode_ === 'Compact';
  }

  private computeShowContextEntrypoint_(): boolean {
    if (this.hideContextButton_ || !this.isAimPopupEligible_) {
      return false;
    }

    if (this.searchboxLayoutMode_.startsWith('Tall')) {
      return true;
    }

    if (this.searchboxLayoutMode_ === 'Compact') {
      return this.isLensSearchEligible_;
    }

    return false;
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
      const available = this.getAvailableSelections(this.result_);
      const sameLineSelection = {
        ...this.selection,
        state: SelectionLineState.kNormal,
      };
      if (result.matches[0]?.allowedToBeDefaultMatch) {
        this.setSelection(available[0] || kDefaultSelection);
      } else if (available.some(s => selectionsEqual(s, sameLineSelection))) {
        this.setSelection(sameLineSelection);
      } else {
        this.setSelection(kDefaultSelection);
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
    if (this.showContextEntrypoint_ && !this.shouldHideEntrypointButton_()) {
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
    this.refreshCurrentTabForChip_();
  }

  protected onDropdownDomChange_() {
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
      this.setSelection(selection, false);
    } else {
      this.getDropdown().updateSelection(oldSelection, selection);
    }
  }

  override setSelection(
      selection: OmniboxPopupSelection, notify: boolean = true) {
    const oldSelection = this.selection;
    super.setSelection(selection);
    this.getDropdown().updateSelection(oldSelection, this.selection);
    if (notify) {
      this.searchboxBrowserProxy_.handler.setPopupSelection(
          selectionIsNativelySupported(this.selection) ? this.selection :
                                                         kDefaultSelection);
    }

    const entrypoint = this.getContextualEntrypointButton_();
    if (entrypoint) {
      entrypoint.hasPopupFocus = this.selection.state ===
          SelectionLineState.kFocusedButtonContextEntrypoint;
      if (entrypoint.hasPopupFocus) {
        this.notifyContextualEntrypoint_(entrypoint);
      }
    }
  }

  private notifyContextualEntrypoint_(
      entrypoint: ContextualEntrypointButtonElement) {
    const message = entrypoint.shadowRoot.querySelector('#entrypoint')
                        ?.getAttribute('aria-label');
    if (message) {
      if (entrypoint.ariaNotify) {
        entrypoint.ariaNotify(message);
      } else {
        getA11yAnnouncer(entrypoint).announce(message);
      }
    }
  }

  // Changes the current popup selection to the next selection in the order of
  // all available selections. That is, it translates a user intent (the given
  // `direction` and `step`) into a change of popup-focus (distinct from actual
  // browser/input focus, popup-focus shows what item from the popup will be
  // opened when user presses Enter).
  private stepSelection_(direction: SelectionDirection, step: SelectionStep) {
    if (!this.result_) {
      return;
    }
    this.setSelection(
        this.getNextSelection(this.result_, this.selection, direction, step));
  }

  // Opens the current popup selection (the one visually indicated by the
  // element with popup-focus).
  private openCurrentSelection_(disposition: WindowOpenDisposition) {
    if (this.selection.state ===
        SelectionLineState.kFocusedButtonContextEntrypoint) {
      this.browserProxy_.handler.showContextMenu({x: 0, y: 0});
    } else if (selectionIsNativelySupported(this.selection)) {
      this.searchboxBrowserProxy_.handler.openPopupSelection(
          this.result_?.sequenceId || 0, this.selection, disposition);
    } else {
      assertNotReached(
          `openCurrentSelection_ called for unsupported selection: ${
              selectionToString(this.selection)}`);
    }
  }

  protected onHasSecondarySideChanged_(e: CustomEvent<{value: boolean}>) {
    this.hasSecondarySide = e.detail.value;
  }

  protected onContextMenuEntrypointClick_(
      e: CustomEvent<{x: number, y: number}>) {
    e.preventDefault();
    const point = {
      x: e.detail.x,
      y: e.detail.y,
    };

    // Force the button to keep its hover background visually while
    // the menu is open, even if the mouse doesn't move out of the button
    // area after clicking.
    const contextButton = this.getContextualEntrypointButton_();
    if (contextButton) {
      contextButton.classList.add('menu-open');
    }
    this.browserProxy_.handler.showContextMenu(point);
  }

  private onContextMenuClosed_() {
    const contextButton = this.getContextualEntrypointButton_();
    if (contextButton) {
      contextButton.classList.remove('menu-open');
    }
  }

  protected onLensSearchClick_() {
    this.searchboxBrowserProxy_.handler.openLensSearch();
  }

  protected async refreshCurrentTabForChip_() {
    // TODO (b/537859769) - Replace getRecentTabs with a dedicated handler for
    // current tab chip.
    const {tabs} = await this.searchboxBrowserProxy_.handler.getRecentTabs();
    this.currentTabForChip_ =
        tabs.find(tab => tab.showInCurrentTabChip) || null;
  }

  protected onAddTabContext_(e: CustomEvent<{
    id: number,
    title: string,
    url: Url,
  }>) {
    this.searchboxBrowserProxy_.handler.addTabContext(
        e.detail.id, /*delayUpload=*/ false);
  }

  protected computeShowCurrentTabChip_() {
    const browserTabsAllowedByPecApi = !this.usePecApi_ ||
        (!!this.inputState_ &&
         this.inputState_.allowedInputTypes.includes(InputType.kBrowserTab));
    return this.isCurrentTabChipEnabled_ && !!this.currentTabForChip_ &&
        browserTabsAllowedByPecApi;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'omnibox-popup-app': OmniboxPopupAppElement;
  }
}

customElements.define(OmniboxPopupAppElement.is, OmniboxPopupAppElement);
