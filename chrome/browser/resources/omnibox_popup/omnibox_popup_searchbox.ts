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
import {sanitizeTextForPaste} from './utils.js';

/**
 * Focus actions deferred when `document.visibilityState` is hidden.
 * - FOCUS: Focus only (set by `onSetInputState_` where `selectRange()` handles
 * selection separately).
 * - FOCUS_AND_SELECT: Focus and select all text (set by `onSetFocus_` when
 * refocusing).
 */
enum DeferredFocusAction {
  FOCUS,
  FOCUS_AND_SELECT,
}

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
  // TODO(b/504669677): Replace `deferredFocusAction_` with
  // an actual handshake, to give more control of when focusing happens, instead
  // of relying on deferred focus/selectall.
  // Stores pending focus action if focus arrives while document is hidden.
  private deferredFocusAction_: DeferredFocusAction|null = null;

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
      this.popupCallbackRouter_.setFocus.addListener(
          this.onSetFocus_.bind(this)),
    ];
    this.eventTracker_.add(
        document, 'selectionchange', this.onSelectionChanged_.bind(this));
    this.eventTracker_.add(
        document, 'visibilitychange', this.onVisibilityChange_.bind(this));
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

    // Request initial native state in case C++ synced before WebUI connected
    // (e.g., if WebUI preloading is disabled).
    this.popupPageHandler_.requestInputState();
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

  /**
   * Clears frontend autocomplete matches and reverts C++ `OmniboxEditModel`.
   * Kept separate from `clearAutocompleteMatches()`, which is called internally
   * by `SearchboxMixin` when selecting suggestions or blurring the input.
   */
  revert() {
    this.clearAutocompleteMatches();
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
    return super.isAutocompleteResultStale(result) || !result.matches.length;
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
      const input = this.getInputElement().inputElement;
      if (input.selectionStart === 0 &&
          input.selectionEnd === input.value.length) {
        this.showFullUrlOnDeselect_();
        if (e.detail.key === 'ArrowLeft') {
          input.setSelectionRange(0, 0);
        } else {
          input.setSelectionRange(input.value.length, input.value.length);
        }
      }
    }
  }

  protected onInputPaste_(e: ClipboardEvent) {
    let text = e.clipboardData?.getData('text/plain');
    if (!text && e.clipboardData?.types.includes('text/x-moz-url')) {
      // 'text/x-moz-url' is formatted as "URL\nTitle" when copying bookmarks.
      // Extract the URL part, while dropping the Title part.
      // This logic aligns with the native `GetClipboardText()` behavior in
      // Views.
      text = e.clipboardData.getData('text/x-moz-url').split(/\r?\n/)[0];
    }
    if (!text) {
      return;
    }

    e.preventDefault();
    const sanitizedText = sanitizeTextForPaste(text);

    const input = this.getInputElement().inputElement;
    const start = input.selectionStart || 0;
    const end = input.selectionEnd || 0;
    const newValue = input.value.substring(0, start) + sanitizedText +
        input.value.substring(end);

    this.getInputElement().setInput({text: newValue, inline: ''});
    const cursorPos = start + sanitizedText.length;
    this.getInputElement().setSelectionRange(cursorPos, cursorPos);
    this.getInputElement().dispatchEvent(
        new CustomEvent('searchbox-input-text-updated', {
          detail: {
            value: newValue,
            isComposing: false,
          },
        }));
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
      this.maybeShowFullUrl_();
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
      if (document.visibilityState === 'visible') {
        this.deferredFocusAction_ = null;
        this.$.input.focus();
      } else {
        this.deferredFocusAction_ = DeferredFocusAction.FOCUS;
      }
    } else {
      this.deferredFocusAction_ = null;
      this.$.input.blur();
    }
    if (state.showFullUrl) {
      this.maybeShowFullUrl_();
    } else {
      this.fullUrlShown_ = false;
    }

    // Input gets focused on init which triggers Blink's
    // `UpdateSelectionOnFocus`. Set `pendingFocusSelection_` so that this
    // update does not trigger `onSelectionChanged()`. See line 348 of
    // third_party/blink/renderer/core/html/forms/html_input_element.cc.
    const isAlreadyFocused = this.shadowRoot?.activeElement === this.$.input;
    this.pendingFocusSelection_ =
        (state.isFocused && !isAlreadyFocused) ? state.selection : null;
    this.selectRange(state.selection);
    this.getDropdownElement().unselect();
    this.pageHandler().stopAutocomplete(/*clearResult=*/ false);
  }

  /**
   * Called by C++ via `SetFocus` Mojo IPC when the browser refocuses the
   * Omnibox while the popup is already open (or during tab restoration). If the
   * document is visible, focuses and selects all input text immediately.
   * If hidden, defers the action until `visibilitychange`.
   */
  private onSetFocus_(isFocused: boolean) {
    if (isFocused) {
      if (document.visibilityState === 'visible') {
        this.deferredFocusAction_ = null;
        this.$.input.focus();
        this.getInputElement().select();
      } else {
        // Defer focusing and selecting text if the document is currently
        // hidden, as DOM focus calls on hidden documents may be ignored.
        this.deferredFocusAction_ = DeferredFocusAction.FOCUS_AND_SELECT;
      }
    } else {
      this.deferredFocusAction_ = null;
      this.$.input.blur();
    }
  }

  /**
   * Applies `selection` to `#input`. If a partial sub-range is requested while
   * displaying an elided URL, unelides `fullUrl_` first before selecting.
   */
  private selectRange(selection: {start: number, end: number}) {
    const {start, end} = selection;
    const input = this.getInputElement().inputElement.value;
    const isFullSelection =
        Math.abs(start - end) === input.length && input.length > 0;

    // If a partial sub-range (e.g. from double click or drag) is requested
    // while displaying an elided URL, unelide to `fullUrl_` before applying
    // the selection so the highlight matches full URL coordinates exactly.
    if (start !== end && !isFullSelection && this.fullUrl_) {
      this.maybeShowFullUrl_();
    }

    if (isFullSelection) {
      this.getInputElement().select();
    } else {
      this.$.input.setSelectionRange(
          Math.min(start, end), Math.max(start, end));
    }
  }

  private maybeShowFullUrl_() {
    if (this.userInputInProgress_) {
      return;
    }
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

  /**
   * Executes deferred focus (`FOCUS` / `FOCUS_AND_SELECT`) when the tab
   * transitions from hidden to visible (`document.visibilityState ===
   * 'visible'`).
   */
  private onVisibilityChange_() {
    if (document.visibilityState === 'visible' &&
        this.deferredFocusAction_ !== null) {
      const action = this.deferredFocusAction_;
      this.deferredFocusAction_ = null;
      this.$.input.focus();
      if (action === DeferredFocusAction.FOCUS_AND_SELECT) {
        this.getInputElement().select();
      }
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
    if (!e.detail.value.trim()) {
      // Notify the backend when the user clears all input (`onInputCleared`) so
      // it knows the draft was manually cleared and can revert empty drafts on
      // tab switch (restoring the permanent URL instead of a blank string).
      this.clearAutocompleteMatches();
      this.popupPageHandler_.onInputCleared(this.currentSequenceNum_);
    } else {
      this.onSearchboxInputTextUpdated(e, /*forceAutocomplete=*/ false);
    }
  }

  override onInputWrapperFocusout(e: FocusEvent) {
    // Note: super.onInputWrapperFocusout calls
    // this.pageHandler().onFocusChanged(false), which dispatches to
    // searchboxPageHandler_.onFocusChanged(false) via our pageHandler()
    // override.
    super.onInputWrapperFocusout(e);
    this.$.input.blur();
  }

  protected onVoiceSearchClick_() {
    this.dispatchEvent(new Event('open-voice-search'));
  }

  protected onLensSearchClick_() {
    this.dropdownIsVisible = false;
    this.dispatchEvent(new Event('open-lens-search'));
  }

  override async handleKeyNavigation(e: KeyboardEvent) {
    // TODO(b/514810983): Reconcile escape behavior with that of the Views
    // Omnibox.
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
          this.revert();
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
