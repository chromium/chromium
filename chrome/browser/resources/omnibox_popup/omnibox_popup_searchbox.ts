// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_components/searchbox/searchbox_dropdown.js';
import '//resources/cr_components/searchbox/searchbox_input.js';
import '//resources/cr_components/searchbox/searchbox_compose_button.js';

import {SearchboxBrowserProxy} from '//resources/cr_components/searchbox/searchbox_browser_proxy.js';
import type {ComposeClickEventDetail, SearchboxComposeButtonElement} from '//resources/cr_components/searchbox/searchbox_compose_button.js';
import type {SearchboxDropdownElement} from '//resources/cr_components/searchbox/searchbox_dropdown.js';
import type {SearchboxInputElement} from '//resources/cr_components/searchbox/searchbox_input.js';
import {kDefaultSelection} from '//resources/cr_components/searchbox/searchbox_match.js';
import type {SearchboxMixinInterface} from '//resources/cr_components/searchbox/searchbox_mixin.js';
import {SearchboxMixin} from '//resources/cr_components/searchbox/searchbox_mixin.js';
import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {WebUiListenerMixinLit} from '//resources/cr_elements/web_ui_listener_mixin_lit.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import {SelectionLineState} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {AutocompleteResult, PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerInterface as SearchboxPageHandlerInterface} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {Url} from '//resources/mojo/url/mojom/url.mojom-webui.js';

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

/**
 * 675px ~= 449px (--cr-realbox-primary-side-min-width) * 1.5 + some margin.
 */
const canShowSecondarySideMediaQueryList =
    window.matchMedia('(min-width: 675px)');

export interface AimButtonConfig {
  text: string;
  title: string;
  a11yLabel: string;
  icon: string;
}

export interface OmniboxPopupSearchboxElement {
  $: {
    composeButton: SearchboxComposeButtonElement,
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
      omniboxPopupDebugEnabled_: {
        type: Boolean,
        reflect: true,
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
      multiLineEnabled: {
        type: Boolean,
        reflect: true,
      },
      searchboxDynamicColorScheme_: {
        type: Boolean,
        reflect: true,
      },
      hasUserInput_: {
        type: Boolean,
      },
      searchboxDynamicAnimation_: {
        type: Boolean,
      },
      aimButtonVisible_: {
        type: Boolean,
      },
      aimButtonConfig_: {
        type: Object,
      },
      /**
       * Whether the secondary side can be shown based on the feature state and
       * the width available to the dropdown.
       */
      canShowSecondarySide: {
        type: Boolean,
        reflect: true,
      },
      /**
       * Whether the secondary side is currently available to be shown.
       */
      hasSecondarySide: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  accessor canShowSecondarySide: boolean =
      canShowSecondarySideMediaQueryList.matches;
  accessor hasSecondarySide: boolean = false;
  accessor searchboxChromeRefreshTheming: boolean =
      loadTimeData.getBoolean('searchboxCr23Theming');
  accessor searchboxSteadyStateShadow: boolean =
      loadTimeData.getBoolean('searchboxCr23SteadyStateShadow');
  accessor omniboxPopupDebugEnabled_: boolean =
      loadTimeData.getBoolean('omniboxPopupDebugEnabled');
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
  protected accessor searchboxDynamicColorScheme_: boolean =
      loadTimeData.getBoolean('searchboxDynamicColorScheme');
  protected accessor searchboxDynamicAnimation_: boolean =
      loadTimeData.getBoolean('searchboxDynamicAnimation');
  protected accessor hasUserInput_: boolean = false;
  protected accessor aimButtonVisible_: boolean = false;
  protected accessor aimButtonConfig_: AimButtonConfig = {
    text: '',
    title: '',
    a11yLabel: '',
    icon: '',
  };

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
  private permanentDisplayText_: string = '';
  private fullUrlShown_: boolean = false;
  // TODO(b/504669677): Replace `deferredFocusAction_` with
  // an actual handshake, to give more control of when focusing happens, instead
  // of relying on deferred focus/selectall.
  // Stores pending focus action if focus arrives while document is hidden.
  private deferredFocusAction_: DeferredFocusAction|null = null;
  // Used to signify that on mouseup, the default action of `unselect()`
  // should be ignored.
  private selectAllOnMouseRelease_: boolean = false;

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
      this.searchboxCallbackRouter_.setAimButtonVisible.addListener(
          (visible: boolean) => {
            this.aimButtonVisible_ = visible;
          }),
      this.searchboxCallbackRouter_.setAimButtonConfig.addListener(
          (text: string, tooltip: string, a11yLabel: string, iconUrl: Url) => {
            this.aimButtonConfig_ = {
              text,
              title: tooltip,
              a11yLabel,
              icon: iconUrl,
            };
          }),
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

    // When `selectAllOnMouseRelease_` is true (set during `onInputMousedown_`
    // when the input is focused and the selection is collapsed), prevent the
    // default `mouseup` behavior. This stops the text from being unselected
    // after a full selection was programmatically applied during `mousedown`.
    this.eventTracker_.add(
        this.$.input.inputElement, 'mouseup', (e: MouseEvent) => {
          if (this.shadowRoot?.activeElement === this.$.input &&
              this.selectAllOnMouseRelease_) {
            this.selectAllOnMouseRelease_ = false;
            e.preventDefault();
          }
        });

    this.eventTracker_.add(
        canShowSecondarySideMediaQueryList, 'change',
        this.onCanShowSecondarySideChanged_.bind(this));

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

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('selectedMatchIndex')) {
      // Guard against transient out-of-bounds indices when autocomplete results
      // are being cleared or updated asynchronously. The backend will be synced
      // once the new valid results are rendered.
      if (this.selectedMatchIndex !== -1 &&
          (!this.result || !this.result.matches ||
           this.selectedMatchIndex >= this.result.matches.length)) {
        return;
      }
      // Synchronize selection changes driven by WebUI back to C++. This
      // ensures the backend edit model is aware of the active selection and can
      // preserve it across tab switches.
      this.searchboxPageHandler_.setPopupSelection(
          this.selectedMatchIndex === -1 ? kDefaultSelection : {
            line: this.selectedMatchIndex,
            state: SelectionLineState.kNormal,
            actionIndex: 0,
          });
    }
  }

  override shouldAppendDotComOnCtrlEnter(): boolean {
    return true;
  }

  override isBackgroundTabNavigation(e: KeyboardEvent|MouseEvent): boolean {
    // Duplicate logic from
    // `searchbox::ComputeOpenDispositionFromModifiersAndLogToUma()` to
    // determine if a background tab is opened.
    return (e.altKey && e.shiftKey) || (e.metaKey && !e.shiftKey);
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

  override onInputFocusChanged(
      e: CustomEvent<{value: string, isOnFocus: boolean}>) {
    // Don't populate results if the user edited the input.
    if (this.userInputInProgress_ || this.isChromeScheme_()) {
      return;
    }

    // Unlike other searchboxes where emptiness indicates an unedited input,
    // this searchbox may contain prepopulated text (e.g., the page's URL).
    // Therefore, explicitly set `isOnFocus` to true for all focus queries.
    e.detail.isOnFocus = true;
    super.onInputFocusChanged(e);
  }

  protected onInputMousedown_(e: MouseEvent) {
    // If the full url is currently selected, a second mouse click should
    // show the full url.
    this.showFullUrlOnDeselect_();
    // If nothing is selected, a mouse click should select all the text
    // if the input is not already focused. (i.e. focusing on omnibox).
    if (!this.dropdownIsVisible &&
        this.shadowRoot?.activeElement !== this.$.input) {
      // Only handle left (0) and middle (1) mouse button clicks.
      if (e.button === 0 || e.button === 1) {
        const input = this.getInputElement().inputElement;
        if (input.selectionStart === input.selectionEnd) {
          this.selectAllOnMouseRelease_ = true;
          input.select();
        }
      }
    }
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
    if (this.shadowRoot.activeElement !== this.$.input || this.isComposing_) {
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
    this.hasUserInput_ = !!state.text.trim();
    this.currentSequenceNum_ = state.sequenceNumber;
    this.fullUrl_ = state.fullUrl;
    this.lastQueriedInput = state.text;
    this.permanentDisplayText_ = state.permanentDisplayText;
    this.isComposing_ = false;

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

    this.selectRange(state.selection);
    this.getDropdownElement().unselect();
    // If zero-prefix suggestions are requested by the new state, initiate
    // an on-focus autocomplete query. Otherwise, clear any existing matches
    // so the suggestion dropdown is closed on state resets and tab switches.
    if (state.queryZps) {
      this.queryAutocomplete(
          state.text, /*preventInlineAutocomplete=*/ false,
          /*isOnFocus=*/ true);
    } else {
      this.clearAutocompleteMatches();
      this.lastQueriedInput = state.text;
    }
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
      this.handleFocusLost_();
    }
  }

  /**
   * Resets selection range, blurs input, and clears autocomplete matches when
   * focus is lost to external targets or when clicking outside.
   */
  private handleFocusLost_() {
    this.getInputElement().setSelectionRange(0, 0);
    this.getInputElement().blur();
    // Clear autocomplete results so clicking into omnibox_view_views
    // registers that the popup is closed. This enables
    // select_all_on_mouse_release_ (in omnibox_view_views) to be set to the
    // correct value.
    this.clearAutocompleteMatches();
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
    this.fullUrlShown_ = true;
    if (this.userInputInProgress_) {
      return;
    }
    this.$.input.setInputText(this.fullUrl_);
  }

  protected onInputFocusin_() {
    this.searchboxPageHandler_.onFocusChanged(true);
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

  protected onSearchboxInputTextUpdated_(
      e: CustomEvent<{value: string, isComposing: boolean}>) {
    this.userInputInProgress_ = true;
    this.hasUserInput_ = !!e.detail.value.trim();
    if (!e.detail.value.trim()) {
      // Notify the backend when the user clears all input (`onInputCleared`) so
      // it knows the draft was manually cleared and can revert empty drafts on
      // tab switch (restoring the permanent URL instead of a blank string).
      this.clearAutocompleteMatches();
      this.popupPageHandler_.onInputCleared(this.currentSequenceNum_);
    } else {
      this.onSearchboxInputTextUpdated(e);
    }
  }

  override onInputWrapperFocusout(e: FocusEvent) {
    // When focus leaves the WebUI DOM (e.g. to native window or during ESC key
    // handling on Linux), `relatedTarget` is null. Focus loss to external
    // targets is managed by C++ (OmniboxPopupFullPresenter) via
    // `SetFocus(false)` Mojo IPC.
    if (e.relatedTarget === null) {
      return;
    }

    const newlyFocusedEl = e.relatedTarget as Element;
    // Note: super.onInputWrapperFocusout calls
    // this.pageHandler().onFocusChanged(false), which dispatches to
    // searchboxPageHandler_.onFocusChanged(false) via our pageHandler()
    // override.
    super.onInputWrapperFocusout(e);

    const isOutside = !this.getWrapperElement().contains(newlyFocusedEl);
    if (isOutside) {
      this.handleFocusLost_();
    }
  }

  protected onVoiceSearchClick_() {
    this.dispatchEvent(new Event('open-voice-search'));
  }

  protected onLensSearchClick_() {
    this.dropdownIsVisible = false;
    this.dispatchEvent(new Event('open-lens-search'));
  }

  protected onComposeClick_(e: CustomEvent<ComposeClickEventDetail>) {
    this.dropdownIsVisible = false;
    this.popupPageHandler_.openAimPopup(e.detail?.viaKeyboard || false);
  }

  protected onHasSecondarySideChanged_(e: CustomEvent<{value: boolean}>) {
    this.hasSecondarySide = e.detail.value;
  }

  private onCanShowSecondarySideChanged_(e: MediaQueryListEvent) {
    this.canShowSecondarySide = e.matches;
  }

  override handleKeyNavigation(e: KeyboardEvent) {
    // Ignore key navigation (including ESC) during active IME text composition
    // (e.g. Japanese/Chinese/Korean) so the OS IME engine handles the key
    // first.
    if (e.isComposing) {
      return;
    }

    if (e.key === 'Escape') {
      e.preventDefault();
      this.handleEscapeKey_();
    } else {
      super.handleKeyNavigation(e);
    }
  }

  private handleEscapeKey_() {
    const dropdown = this.getDropdownElement();
    const inputEl = this.getInputElement();

    // Stage 1 (`kRevertTemporaryText`): If temporary text active
    // (selectedMatchIndex > 0 or non-default match/action highlighted),
    // restores typed query and resets match selection to index 0. Dropdown
    // stays open and focus stays in Omnibox.
    const hasTemporaryText = this.selectedMatchIndex > 0 ||
        (dropdown && dropdown.selection &&
         dropdown.selection.state !== SelectionLineState.kNormal);
    if (this.dropdownIsVisible && hasTemporaryText) {
      dropdown.selectFirst();
      this.selectedMatchIndex = 0;
      const defaultMatch = this.result?.matches?.[0];
      const typedText = this.lastQueriedInput ?? '';
      const inlineText =
          (defaultMatch && defaultMatch.allowedToBeDefaultMatch) ?
          defaultMatch.inlineAutocompletion :
          '';
      inputEl.setInput({
        text: typedText,
        inline: inlineText,
        moveCursorToEnd: inlineText.length === 0,
      });
      this.popupPageHandler_.logEscapeAction(
          OmniboxEscapeAction.kRevertTemporaryText);
      return;
    }

    // Stage 2 (`kClosePopup`): Closes suggestion popup dropdown. Typed input
    // and focus remain in Omnibox.
    if (this.dropdownIsVisible) {
      this.clearAutocompleteMatches();
      this.popupPageHandler_.logEscapeAction(OmniboxEscapeAction.kClosePopup);
      return;
    }

    // Stage 3 (`kClearUserInput`): Reverts text to permanent page URL and
    // selects all text (or closes UI if already empty on NTP). Focus stays in
    // Omnibox.
    const isInputDirty = this.userInputInProgress_ ||
        inputEl.inputElement.value !== this.permanentDisplayText_;
    if (isInputDirty) {
      const wasAlreadyEmpty = inputEl.inputElement.value.length === 0;
      const restoredText = this.permanentDisplayText_;
      this.fullUrlShown_ = false;
      inputEl.setInput({
        text: restoredText,
        inline: '',
      });
      inputEl.select();
      this.userInputInProgress_ = false;
      this.popupPageHandler_.revert(this.currentSequenceNum_);
      this.popupPageHandler_.logEscapeAction(
          OmniboxEscapeAction.kClearUserInput);
      // If restoring an empty permanent URL (e.g. on NTP) when input was
      // already empty, also close the UI to avoid an invisible "" -> ""
      // update.
      if (restoredText.length === 0 && wasAlreadyEmpty) {
        inputEl.blur();
        this.popupPageHandler_.closeUI();
      }
      return;
    }

    // Stage 4 (`kBlur`): Blurs Omnibox and closes UI, returning focus to Web
    // Contents.
    inputEl.blur();
    this.popupPageHandler_.closeUI();
    this.popupPageHandler_.logEscapeAction(OmniboxEscapeAction.kBlur);
    return;
  }
  }

declare global {
  interface HTMLElementTagNameMap {
    'omnibox-popup-searchbox': OmniboxPopupSearchboxElement;
  }
}

customElements.define(
    OmniboxPopupSearchboxElement.is, OmniboxPopupSearchboxElement);
