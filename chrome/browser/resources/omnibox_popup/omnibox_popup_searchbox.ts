// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_components/searchbox/searchbox_dropdown.js';
import '//resources/cr_components/searchbox/searchbox_input.js';

import {SearchboxBrowserProxy} from '//resources/cr_components/searchbox/searchbox_browser_proxy.js';
import type {SearchboxDropdownElement} from '//resources/cr_components/searchbox/searchbox_dropdown.js';
import type {SearchboxInputElement} from '//resources/cr_components/searchbox/searchbox_input.js';
import type {SearchboxMixinInterface} from '//resources/cr_components/searchbox/searchbox_mixin.js';
import {SearchboxMixin} from '//resources/cr_components/searchbox/searchbox_mixin.js';
import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {WebUiListenerMixinLit} from '//resources/cr_elements/web_ui_listener_mixin_lit.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {AutocompleteResult, PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerInterface as SearchboxPageHandlerInterface} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';

import {browserProxyFactory, OmniboxEscapeAction} from './omnibox_popup.mojom-webui.js';
import type {OmniboxInputState, PageCallbackRouter as PopupPageCallbackRouter, PageHandlerInterface as PopupPageHandlerInterface} from './omnibox_popup.mojom-webui.js';
import {getCss} from './omnibox_popup_searchbox.css.js';
import {getHtml} from './omnibox_popup_searchbox.html.js';

export interface OmniboxPopupSearchboxElement {
  $: {
    input: SearchboxInputElement,
    inputWrapper: HTMLElement,
    matches: SearchboxDropdownElement,
  };
}

// TODO(crbug.com/497883783): Move I18nMixinLit to SearchboxMixin.
const OmniboxPopupSearchboxElementBase =
    SearchboxMixin(I18nMixinLit(WebUiListenerMixinLit(CrLitElement)));

export class OmniboxPopupSearchboxElement extends
    OmniboxPopupSearchboxElementBase implements SearchboxMixinInterface {
  static get is() {
    return 'omnibox-popup-searchbox';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      placeholderText: {
        type: String,
        reflect: true,
        notify: true,
      },
      searchboxChromeRefreshTheming: {
        type: Boolean,
        reflect: true,
      },
      searchboxSteadyStateShadow: {
        type: Boolean,
        reflect: true,
      },
      searchboxIcon_: {type: String},
      searchboxVoiceSearchEnabled_: {
        type: Boolean,
        reflect: true,
      },
      searchboxLensSearchEnabled_: {
        type: Boolean,
        reflect: true,
      },
      useWebkitSearchIcons_: {
        type: Boolean,
        reflect: true,
      },
      // TODO(b/517218130): Ensure Omnibox is laid out correctly when
      //   `isTouchUi_` is true.
      isTouchUi_: {
        type: Boolean,
        reflect: true,
      },
      omniboxPopupDebugEnabled_: {
        type: Boolean,
        reflect: true,
      },
      multiLineEnabled: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  accessor placeholderText: string = '';
  accessor searchboxChromeRefreshTheming: boolean =
      loadTimeData.getBoolean('searchboxCr23Theming');
  accessor searchboxSteadyStateShadow: boolean =
      loadTimeData.getBoolean('searchboxCr23SteadyStateShadow');
  protected accessor searchboxIcon_: string =
      loadTimeData.getString('searchboxDefaultIcon');
  protected accessor searchboxVoiceSearchEnabled_: boolean =
      loadTimeData.getBoolean('searchboxVoiceSearch');
  protected accessor searchboxLensSearchEnabled_: boolean =
      loadTimeData.getBoolean('searchboxLensSearch');
  protected accessor useWebkitSearchIcons_: boolean = false;
  override accessor multiLineEnabled: boolean =
      loadTimeData.getBoolean('searchboxMultiline');
  // TODO(b/519185419): Remove `isTouchUi_` property and from `loadTimeData` and
  // get layout constants and font sizes from a C++ layout helper instead.
  protected accessor isTouchUi_: boolean = loadTimeData.getBoolean('isTouchUi');
  protected accessor omniboxPopupDebugEnabled_: boolean =
      loadTimeData.getBoolean('omniboxPopupDebugEnabled');

  private eventTracker_ = new EventTracker();
  private searchboxPageHandler_: SearchboxPageHandlerInterface;
  private searchboxCallbackRouter_: SearchboxPageCallbackRouter;
  private popupCallbackRouter_: PopupPageCallbackRouter;
  private popupPageHandler_: PopupPageHandlerInterface;
  private listenerIds_: number[] = [];
  private popupListenerIds_: number[] = [];
  // Sequence number of the current content state received from C++.
  private currentSequenceNum_: number = 0;
  // True if the user has modified the text in the input field (e.g., typed or
  // deleted characters), as opposed to displaying permanent text set from C++.
  private userInputInProgress_: boolean = false;
  // True during an active IME (Input Method Editor) text composition session.
  // Used to suppress intermediate selection updates until composition finishes.
  private isComposing_: boolean = false;
  private fullUrl_: string = '';
  private pendingFocusSelection_: {start: number, end: number}|null = null;
  private permanentDisplayText_: string = '';
  private fullUrlShown_: boolean = false;

  constructor() {
    super();
    const searchboxBrowserProxy = SearchboxBrowserProxy.getInstance();
    this.searchboxPageHandler_ = searchboxBrowserProxy.handler;
    this.searchboxCallbackRouter_ = searchboxBrowserProxy.callbackRouter;
    const popupBrowserProxy = browserProxyFactory.getInstance();
    this.popupCallbackRouter_ = popupBrowserProxy.callbackRouter;
    this.popupPageHandler_ = popupBrowserProxy.handler;
  }

  override connectedCallback() {
    super.connectedCallback();
    // TODO(crbug.com/497883783): Move autocompleteResultChangedListenerId_
    // property to SearchboxMixin.
    this.listenerIds_ = [
      this.searchboxCallbackRouter_.autocompleteResultChanged.addListener(
          this.onAutocompleteResultChanged.bind(this)),
    ];
    this.popupListenerIds_ = [
      this.popupCallbackRouter_.setInputState.addListener(
          this.onSetInputState_.bind(this)),
    ];
    this.eventTracker_.add(
        document, 'selectionchange', this.onSelectionChanged_.bind(this));
    // TODO(b/522957982): Establish closer IME parity with the native Views
    // Omnibox (e.g., render inline autocompletion in a separate overlaid span
    // rather than modifying input value during active composition).
    this.eventTracker_.add(this.$.input, 'compositionstart', () => {
      this.isComposing_ = true;
    });
    this.eventTracker_.add(this.$.input, 'compositionend', () => {
      this.isComposing_ = false;
      this.onSelectionChanged_();
    });
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.listenerIds_.forEach(
        id => this.searchboxCallbackRouter_.removeListener(id));
    this.listenerIds_ = [];
    this.popupListenerIds_.forEach(
        id => this.popupCallbackRouter_.removeListener(id));
    this.popupListenerIds_ = [];
    this.eventTracker_.removeAll();
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('searchboxChromeRefreshTheming')) {
      this.useWebkitSearchIcons_ = this.searchboxChromeRefreshTheming;
    }
  }

  override firstUpdated(changedProperties: PropertyValues<this>) {
    super.firstUpdated(changedProperties);
    this.initialInputScrollHeight = this.$.input.inputElement.scrollHeight;
  }

  focusInput() {
    this.$.input.focus();
  }

  //========================================================================
  // SearchboxMixin abstract method implementations
  //========================================================================

  override getInputElement(): SearchboxInputElement {
    return this.$.input;
  }

  override getDropdownElement(): SearchboxDropdownElement {
    return this.$.matches;
  }

  override getWrapperElement(): HTMLElement {
    return this.$.inputWrapper;
  }

  override pageHandler(): SearchboxPageHandlerInterface {
    return this.searchboxPageHandler_;
  }

  override clearAutocompleteMatches() {
    super.clearAutocompleteMatches();
    // Revert the `OmniboxEditModel` to the permanent URL (with sequence guard).
    this.popupPageHandler_.revert(this.currentSequenceNum_);
  }

  // TODO(crbug.com/528331161): Unify this with the NTP searchbox logic and move
  // it to SearchboxMixin.
  override updateDropdownVisibility() {
    super.updateDropdownVisibility();

    if (this.multiLineEnabled && this.dropdownIsVisible) {
      const shouldSuppressDropdown = this.initialInputScrollHeight > 0 &&
          this.$.input.inputElement.scrollHeight >
              this.initialInputScrollHeight;
      if (shouldSuppressDropdown) {
        this.dropdownIsVisible = false;
      }
    }
  }

  isInputEmpty(): boolean {
    // If this is called before first render, the input element will not exist.
    if (!this.shadowRoot?.querySelector('#input') || !this.$.input ||
        !this.$.input.lastInput()) {
      return true;
    }
    return !this.$.input.lastInput()!.text.trim();
  }

  protected shouldShowVoiceLens_(isEnabled: boolean): boolean {
    if (!isEnabled) {
      return false;
    }

    if (!this.isInputEmpty()) {
      return false;
    }

    return true;
  }

  override isAutocompleteResultStale(result: AutocompleteResult): boolean {
    return (!!this.lastQueriedInput &&
            this.lastQueriedInput.trimStart() !== result.input) ||
        !result.matches.length;
  }

  //========================================================================
  // Event handlers
  //========================================================================

  override onInputFocusChanged(e: CustomEvent<{value: string}>) {
    // Don't populate results if the user edited the input.
    if (this.userInputInProgress_ || this.isChromeScheme_()) {
      return;
    }
    super.onInputFocusChanged(e);
  }

  protected onInputMousedown_() {
    this.showFullUrlOnDeselect_();
  }

  protected onInputKeydown_(e: CustomEvent<{key: string}>) {
    if (e.detail.key === 'ArrowLeft' || e.detail.key === 'ArrowRight') {
      this.showFullUrlOnDeselect_();
      const input = this.getInputElement().inputElement;
      if (e.detail.key === 'ArrowLeft') {
        input.setSelectionRange(0, 0);
      } else {
        input.setSelectionRange(input.value.length, input.value.length);
      }
    }
  }

  protected showFullUrlOnDeselect_() {
    if (this.userInputInProgress_) {
      return;
    }
    const input = this.getInputElement().inputElement;
    if (input.selectionEnd === null || input.selectionStart === null) {
      return;
    }
    if (input.selectionEnd - input.selectionStart === input.value.length) {
      this.showFullUrl_();
    }
  }

  private isChromeScheme_(): boolean {
    try {
      const url = new URL(this.fullUrl_);
      return url.protocol === 'chrome:' || url.protocol === 'chrome-untrusted:';
    } catch (e) {
      // Invalid URL string
      return false;
    }
  }

  /**
   * Reports selection changes back to C++.
   */
  private onSelectionChanged_() {
    const input = this.$.input.inputElement;
    // Suppress selection updates during active IME text composition.
    if (this.shadowRoot.activeElement !== this.$.input || this.isComposing_ ||
        this.pendingFocusSelection_) {
      return;
    }

    this.popupPageHandler_.onSelectionChanged(
        {start: input.selectionStart || 0, end: input.selectionEnd || 0},
        this.currentSequenceNum_, this.fullUrlShown_);
  }

  /**
   * Sets the input text and applies selection range synchronously regardless of
   * focus.
   */
  private onSetInputState_(state: OmniboxInputState) {
    this.$.input.setInputText(state.text);
    this.userInputInProgress_ = state.userInputInProgress;
    this.currentSequenceNum_ = state.sequenceNumber;
    this.fullUrl_ = state.fullUrl;
    this.lastQueriedInput = state.text;
    this.permanentDisplayText_ = state.permanentDisplayText;

    // Clear any stale results and close the dropdown on a hard state reset.
    // Clear results here since focusout event may not fire.
    if (this.result && this.isAutocompleteResultStale(this.result)) {
      this.result = null;
      this.dropdownIsVisible = false;
    }

    if (state.isFocused) {
      this.$.input.focus();
    } else {
      this.$.input.blur();
    }

    if (state.showFullUrl) {
      this.showFullUrl_();
    } else {
      this.fullUrlShown_ = false;
    }

    // Input gets focused on init which triggers Blink's
    // `UpdateSelectionOnFocus`. Set `pendingFocusSelection_` so that this
    // update does not trigger `onSelectionChanged()`. See line 348 of
    // third_party/blink/renderer/core/html/forms/html_input_element.cc.
    this.pendingFocusSelection_ = state.selection;
    this.selectRange(state.selection);
    this.getDropdownElement().unselect();
    this.pageHandler().stopAutocomplete(/*clearResult=*/ false);
  }

  private selectRange(selection: {start: number, end: number}) {
    const {start, end} = selection;
    const input = this.getInputElement().inputElement.value.trim();
    // Selection can come from either direction.
    if (!(start - end === input.length) && !(end - start === input.length)) {
      // Only show the full url if user is on a page with a url and if there is
      // actually some range selected (i.e. not just moving cursor).
      if (start !== end && this.fullUrl_) {
        this.showFullUrl_();
      }
      this.$.input.setSelectionRange(
          Math.min(start, end), Math.max(start, end));
    } else {
      this.$.input.select();
    }
  }

  private showFullUrl_() {
    this.$.input.setInputText(this.fullUrl_);
    this.fullUrlShown_ = true;
  }

  protected onInputFocusin_() {
    this.searchboxPageHandler_.onFocusChanged(true);
    if (this.pendingFocusSelection_) {
      this.selectRange(this.pendingFocusSelection_);
      // Delay clearing pending focus as tab switch sets `pendingFocusSelection`
      // after `focusin` is called.
      requestAnimationFrame(() => {
        this.pendingFocusSelection_ = null;
      });
    }
  }

  protected computePlaceholderText_(): string {
    if (this.placeholderText) {
      return this.placeholderText;
    }
    return this.i18n('searchBoxHint');
  }

  protected onSearchboxInputTextUpdated_(
      e: CustomEvent<{value: string, isComposing: boolean}>) {
    this.userInputInProgress_ = true;
    this.onSearchboxInputTextUpdated(e, /*forceAutocomplete=*/ false);
  }

  override onInputWrapperFocusout(e: FocusEvent) {
    super.onInputWrapperFocusout(e);

    const newlyFocusedEl = e.relatedTarget as Element;
    // Check if the focus has completely left the searchbox wrapper, and not
    // just moved to another internal child element (e.g., the clear button).
    const isOutside = !this.getWrapperElement().contains(newlyFocusedEl);

    // Only trigger a manual blur if the user clicked outside the searchbox
    // within the active window. Avoid blurring if the entire browser window
    // lost focus, which should preserve the Omnibox focus state.
    if (isOutside && document.visibilityState === 'visible') {
      // Pass `currentSequenceNum_` to the C++ handler to prevent stale
      // blur events from previous tabs from incorrectly blurring
      // a newly focused tab during rapid tab switching.
      this.popupPageHandler_.onManualBlur(this.currentSequenceNum_);
      this.$.input.blur();
    }
  }

  protected onVoiceSearchClick_() {
    this.dispatchEvent(new Event('open-voice-search'));
  }

  protected onLensSearchClick_() {
    this.dropdownIsVisible = false;
    this.dispatchEvent(new Event('open-lens-search'));
  }

  override async handleKeyNavigation(e: KeyboardEvent) {
    if (e.key === 'Escape') {
      if (this.dropdownIsVisible) {
        e.preventDefault();
        if (this.selectedMatchIndex > 0) {
          // If there is temporary text (i.e. a non default suggestion is
          // selected), then revert it.
          await this.getDropdownElement().selectFirst();
          this.popupPageHandler_.logEscapeAction(
              OmniboxEscapeAction.kRevertTemporaryText);
        } else {
          // Otherwise, close the popup if it's open.
          this.clearAutocompleteMatches();
          this.popupPageHandler_.logEscapeAction(
              OmniboxEscapeAction.kClosePopup);
        }
      } else {
        if (this.getInputElement().inputElement.value !==
            this.permanentDisplayText_) {
          e.preventDefault();
          // Clear the input by restoring the permanent display text.
          this.getInputElement().setInput({
            text: this.permanentDisplayText_,
            inline: '',
          });
          this.getInputElement().select();
          this.popupPageHandler_.logEscapeAction(
              OmniboxEscapeAction.kClearUserInput);
        } else {
          // Blur the Omnibox popup by closing it.
          this.popupPageHandler_.closeUI();
          this.popupPageHandler_.logEscapeAction(OmniboxEscapeAction.kBlur);
        }
      }
      return;
    }
    await super.handleKeyNavigation(e);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'omnibox-popup-searchbox': OmniboxPopupSearchboxElement;
  }
}

customElements.define(
    OmniboxPopupSearchboxElement.is, OmniboxPopupSearchboxElement);
