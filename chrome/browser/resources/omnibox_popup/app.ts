// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_components/composebox/composebox_lens_search.js';
import '//resources/cr_components/composebox/contextual_entrypoint_button.js';
import '//resources/cr_components/searchbox/searchbox_dropdown.js';
import '//resources/cr_elements/icons.html.js';
import '/strings.m.js';

import {ColorChangeUpdater} from '//resources/cr_components/color_change_listener/colors_css_updater.js';
import type {ContextualEntrypointButtonElement} from '//resources/cr_components/composebox/contextual_entrypoint_button.js';
import {SearchboxBrowserProxy} from '//resources/cr_components/searchbox/searchbox_browser_proxy.js';
import type {SearchboxDropdownElement} from '//resources/cr_components/searchbox/searchbox_dropdown.js';
import {kDefaultSelection} from '//resources/cr_components/searchbox/searchbox_match.js';
import {getMatchSelections, getNextSelection, selectionIsNativelySupported, selectionsEqual, selectionToString} from '//resources/cr_components/searchbox/selection_control.js';
import type {AutocompleteResult, OmniboxPopupSelection, SelectionDirection, SelectionStep} from '//resources/cr_components/searchbox/selection_control.js';
import {SelectionLineState} from '//resources/cr_components/searchbox/selection_control.js';
import {getInstance as getA11yAnnouncer} from '//resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {assertNotReached} from '//resources/js/assert.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {MetricsReporterImpl} from '//resources/js/metrics_reporter/metrics_reporter.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import type {InputState} from '//resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import type {WindowOpenDisposition} from '//resources/mojo/ui/base/mojom/window_open_disposition.mojom-webui.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import type {BrowserProxy} from './omnibox_popup.mojom-webui.js';
import {browserProxyFactory} from './omnibox_popup.mojom-webui.js';

// 675px ~= 449px (--cr-realbox-primary-side-min-width) * 1.5 + some margin.
const canShowSecondarySideMediaQueryList =
    window.matchMedia('(min-width: 675px)');

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
  protected accessor inputState_: InputState|null = null;
  protected accessor usePecApi_: boolean =
      loadTimeData.getBoolean('contextualMenuUsePecApi');
  protected accessor applyContextButtonBackground_: boolean = false;
  protected accessor isOblongShape_: boolean =
      loadTimeData.getBoolean('contextButtonShapeIsOblong');

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
  private selection_: OmniboxPopupSelection = kDefaultSelection;

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
        changedPrivateProperties.has('isLensSearchEligible_')) {
      this.isLensChipShown_ =
          this.isContentSharingEnabled_ && this.isLensSearchEligible_;
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
      this.searchboxBrowserProxy_.handler.setPopupSelection(
          selectionIsNativelySupported(this.selection_) ? this.selection_ :
                                                          kDefaultSelection);
    }

    const entrypoint = this.getContextualEntrypointButton_();
    if (entrypoint) {
      entrypoint.hasPopupFocus = this.selection_.state ===
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
    const available = this.getResultSelections_(this.result_);
    this.setSelection_(
        getNextSelection(this.selection_, direction, step, available));
  }

  // Returns the full set of selections available based on the given
  // AutocompleteResult. Note, this currently also depends on the
  // current popup state, e.g. the AI Mode button visibility, but
  // should eventually be driven entirely by a single data structure.
  private getResultSelections_(result: AutocompleteResult):
      OmniboxPopupSelection[] {
    const available = getMatchSelections(result);
    // TODO(crbug.com/462775253): Ideally everything available for selection
    // comes from the AutocompleteResult.
    if (this.showContextEntrypoint_ && !this.shouldHideEntrypointButton_()) {
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
      this.browserProxy_.handler.showContextMenu({x: 0, y: 0});
    } else if (selectionIsNativelySupported(this.selection_)) {
      this.searchboxBrowserProxy_.handler.openPopupSelection(
          this.result_?.sequenceId || 0, this.selection_, disposition);
    } else {
      assertNotReached(
          `openCurrentSelection_ called for unsupported selection: ${
              selectionToString(this.selection_)}`);
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
}

declare global {
  interface HTMLElementTagNameMap {
    'omnibox-popup-app': OmniboxPopupAppElement;
  }
}

customElements.define(OmniboxPopupAppElement.is, OmniboxPopupAppElement);
