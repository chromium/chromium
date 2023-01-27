// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/omnibox/realbox_dropdown.js';
import 'chrome://resources/cr_components/omnibox/realbox_icon.js';

import {AutocompleteMatch, AutocompleteResult, NavigationPredictor, PageCallbackRouter, PageHandlerInterface} from 'chrome://resources/cr_components/omnibox/omnibox.mojom-webui.js';
import {RealboxBrowserProxy} from 'chrome://resources/cr_components/omnibox/realbox_browser_proxy.js';
import {RealboxDropdownElement} from 'chrome://resources/cr_components/omnibox/realbox_dropdown.js';
import {RealboxIconElement} from 'chrome://resources/cr_components/omnibox/realbox_icon.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {MetricsReporterImpl} from 'chrome://resources/js/metrics_reporter/metrics_reporter.js';
import {hasKeyModifiers} from 'chrome://resources/js/util_ts.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import {decodeString16, mojoString16, mojoTimeDelta} from '../utils.js';

import {getTemplate} from './realbox.html.js';

// 900px ~= 561px (max value for --ntp-search-box-width) * 1.5 + some margin.
const showSecondaryMatchesMediaQueryList =
    window.matchMedia('(min-width: 900px)');

interface Input {
  text: string;
  inline: string;
}

interface InputUpdate {
  text?: string;
  inline?: string;
  moveCursorToEnd?: boolean;
}

export interface RealboxElement {
  $: {
    icon: RealboxIconElement,
    input: HTMLInputElement,
    inputWrapper: HTMLElement,
    matches: RealboxDropdownElement,
    voiceSearchButton: HTMLElement,
  };
}

/** A real search box that behaves just like the Omnibox. */
export class RealboxElement extends PolymerElement {
  static get is() {
    return 'ntp-realbox';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      //========================================================================
      // Public properties
      //========================================================================

      /** Whether secondary matches can be shown. */
      canShowSecondaryMatches: {
        type: Boolean,
        value: () => showSecondaryMatchesMediaQueryList.matches &&
            loadTimeData.getBoolean('showSecondarySide'),
        reflectToAttribute: true,
      },

      /** Whether secondary matches were at any point available to show. */
      hadSecondaryMatches: {
        type: Boolean,
        reflectToAttribute: true,
      },

      /** Whether secondary matches are currently available to show. */
      hasSecondaryMatches: {
        type: Boolean,
        reflectToAttribute: true,
      },

      /** Whether the theme is dark. */
      isDark: {
        type: Boolean,
        reflectToAttribute: true,
      },

      /** Whether matches are currently visible. */
      matchesAreVisible: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      /** Whether the realbox should match the searchbox. */
      matchSearchbox: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('realboxMatchSearchboxTheme'),
        reflectToAttribute: true,
      },

      /** Whether the Google Lens icon should be visible in the searchbox. */
      realboxLensSearchEnabled: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('realboxLensSearch'),
        reflectToAttribute: true,
      },

      /** Whether to display single-colored icons or not. */
      singleColoredIcons: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      //========================================================================
      // Private properties
      //========================================================================

      /**
       * The time of the first character insert operation that has not yet been
       * painted in milliseconds. Used to measure the realbox responsiveness.
       */
      charTypedTime_: {
        type: Number,
        value: 0,
      },

      /**
       * Whether user is deleting text in the input. Used to prevent the default
       * match from offering inline autocompletion.
       */
      isDeletingInput_: {
        type: Boolean,
        value: false,
      },

      /**
       * The 'Enter' keydown event that was ignored due to matches being stale.
       * Used to navigate to the default match once up-to-date matches arrive.
       */
      lastIgnoredEnterEvent_: {
        type: Object,
        value: null,
      },

      /**
       * Last state of the input (text and inline autocompletion). Updated
       * by the user input or by the currently selected autocomplete match.
       */
      lastInput_: {
        type: Object,
        value: {text: '', inline: ''},
      },

      /**
       * The time at which the input was last focused in milliseconds. Passed to
       * the browser when navigating to a match.
       */
      lastInputFocusTime_: {
        type: Number,
        value: null,
      },

      /** The last queried input text. */
      lastQueriedInput_: {
        type: String,
        value: null,
      },

      /**
       * True if user just pasted into the input. Used to prevent the default
       * match from offering inline autocompletion.
       */
      pastedInInput_: {
        type: Boolean,
        value: false,
      },

      /** Realbox default icon (i.e., Google G icon or the search loupe). */
      realboxIcon_: {
        type: String,
        value: () => loadTimeData.getString('realboxDefaultIcon'),
      },

      /**
       * Whether the Google Lens icon should be visible in the searchbox.
       */
      realboxLensSearchEnabled_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('realboxLensSearch'),
        reflectToAttribute: true,
      },

      result_: {
        type: Object,
      },

      /** The currently selected match, if any. */
      selectedMatch_: {
        type: Object,
        computed: `computeSelectedMatch_(result_, selectedMatchIndex_)`,
      },

      /**
       * Index of the currently selected match, if any.
       * Do not modify this. Use <cr-realbox-dropdown> API to change selection.
       */
      selectedMatchIndex_: {
        type: Number,
        value: -1,
      },

      /** The value of the input element's 'aria-live' attribute. */
      inputAriaLive_: {
        type: String,
        computed: `computeInputAriaLive_(selectedMatch_)`,
      },
    };
  }

  canShowSecondaryMatches: boolean;
  isDark: boolean;
  matchesAreVisible: boolean;
  matchSearchbox: boolean;
  realboxLensSearchEnabled: boolean;
  hadSecondaryMatches: boolean;
  singleColoredIcons: boolean;
  private charTypedTime_: number;
  private inputAriaLive_: string;
  private isDeletingInput_: boolean;
  private lastIgnoredEnterEvent_: KeyboardEvent|null;
  private lastInput_: Input;
  private lastInputFocusTime_: number|null;
  private lastQueriedInput_: string|null;
  private pastedInInput_: boolean;
  private realboxIcon_: string;
  private realboxLensSearchEnabled_: boolean;
  private result_: AutocompleteResult|null;
  private selectedMatch_: AutocompleteMatch|null;
  private selectedMatchIndex_: number;

  private pageHandler_: PageHandlerInterface;
  private callbackRouter_: PageCallbackRouter;
  private autocompleteResultChangedListenerId_: number|null = null;

  constructor() {
    performance.mark('realbox-creation-start');
    super();
    this.pageHandler_ = RealboxBrowserProxy.getInstance().handler;
    this.callbackRouter_ = RealboxBrowserProxy.getInstance().callbackRouter;
  }

  private computeInputAriaLive_(): string {
    return this.selectedMatch_ ? 'off' : 'polite';
  }

  override connectedCallback() {
    super.connectedCallback();
    this.autocompleteResultChangedListenerId_ =
        this.callbackRouter_.autocompleteResultChanged.addListener(
            this.onAutocompleteResultChanged_.bind(this));
    showSecondaryMatchesMediaQueryList.addEventListener(
        'change', this.onCanShowSecondaryMatchesChanged_.bind(this));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    assert(this.autocompleteResultChangedListenerId_);
    this.callbackRouter_.removeListener(
        this.autocompleteResultChangedListenerId_);
    showSecondaryMatchesMediaQueryList.removeEventListener(
        'change', this.onCanShowSecondaryMatchesChanged_.bind(this));
  }

  override ready() {
    super.ready();
    performance.measure('realbox-creation', 'realbox-creation-start');
  }

  //============================================================================
  // Callbacks
  //============================================================================

  private onAutocompleteResultChanged_(result: AutocompleteResult) {
    if (this.lastQueriedInput_ === null ||
        this.lastQueriedInput_.trimStart() !== decodeString16(result.input)) {
      return;  // Stale result; ignore.
    }

    this.result_ = result;
    const hasMatches = result?.matches?.length > 0;
    this.matchesAreVisible = hasMatches;

    this.$.input.focus();

    const firstMatch = hasMatches ? this.result_.matches[0] : null;
    if (firstMatch && firstMatch.allowedToBeDefaultMatch) {
      // Select the default match and update the input.
      this.$.matches.selectFirst();
      this.updateInput_({
        text: this.lastQueriedInput_,
        inline: decodeString16(firstMatch.inlineAutocompletion) || '',
      });

      // Navigate to the default up-to-date match if the user typed and pressed
      // 'Enter' too fast.
      if (this.lastIgnoredEnterEvent_) {
        this.navigateToMatch_(0, this.lastIgnoredEnterEvent_);
        this.lastIgnoredEnterEvent_ = null;
      }
    } else if (
        hasMatches && this.selectedMatchIndex_ !== -1 &&
        this.selectedMatchIndex_ < this.result_.matches.length) {
      // Restore the selection and update the input.
      this.$.matches.selectIndex(this.selectedMatchIndex_);
      this.updateInput_({
        text: decodeString16(this.selectedMatch_!.fillIntoEdit),
        inline: '',
        moveCursorToEnd: true,
      });
    } else {
      // Remove the selection and update the input.
      this.$.matches.unselect();
      this.updateInput_({
        inline: '',
      });
    }
  }

  //============================================================================
  // Event handlers
  //============================================================================

  private onCanShowSecondaryMatchesChanged_(e: MediaQueryListEvent) {
    this.canShowSecondaryMatches =
        e.matches && loadTimeData.getBoolean('showSecondarySide');
  }

  private onHeaderFocusin_() {
    // The header got focus. Unselect the selected match and clear the input.
    assert(this.lastQueriedInput_ === '');
    this.$.matches.unselect();
    this.updateInput_({text: '', inline: ''});
  }

  private onInputCutCopy_(e: ClipboardEvent) {
    // Only handle cut/copy when input has content and it's all selected.
    if (!this.$.input.value || this.$.input.selectionStart !== 0 ||
        this.$.input.selectionEnd !== this.$.input.value.length ||
        !this.result_ || this.result_.matches.length === 0) {
      return;
    }

    if (this.selectedMatch_ && !this.selectedMatch_.isSearchType) {
      e.clipboardData!.setData(
          'text/plain', this.selectedMatch_.destinationUrl.url);
      e.preventDefault();
      if (e.type === 'cut') {
        this.updateInput_({text: '', inline: ''});
        this.clearAutocompleteMatches_();
      }
    }
  }

  private onInputFocus_() {
    this.lastInputFocusTime_ = window.performance.now();
  }

  private onInputInput_(e: InputEvent) {
    const inputValue = this.$.input.value;
    const lastInputValue = this.lastInput_.text + this.lastInput_.inline;
    if (lastInputValue === inputValue) {
      return;
    }

    this.updateInput_({text: inputValue, inline: ''});

    const charTyped = !this.isDeletingInput_ && !!inputValue.trim();
    // If a character has been typed, update |charTypedTime_|. Otherwise reset
    // it. If |charTypedTime_| is not 0, there's a pending typed character for
    // which the results have not been painted yet. In that case, keep the
    // earlier time.
    this.charTypedTime_ =
        charTyped ? this.charTypedTime_ || window.performance.now() : 0;

    // If a character has been typed, mark 'CharTyped'. Otherwise clear it. If
    // 'CharTyped' mark already exists, there's a pending typed character for
    // which the results have not been painted yet. In that case, keep the
    // earlier mark.
    const metricsReporter = MetricsReporterImpl.getInstance();
    if (charTyped) {
      if (!metricsReporter.hasLocalMark('CharTyped')) {
        metricsReporter.mark('CharTyped');
      }
    } else {
      metricsReporter.clearMark('CharTyped');
    }

    if (inputValue.trim()) {
      // TODO(crbug.com/1149769): Rather than disabling inline autocompletion
      // when the input event is fired within a composition session, change the
      // mechanism via which inline autocompletion is shown in the realbox.
      this.queryAutocomplete_(inputValue, e.isComposing);
    } else {
      this.clearAutocompleteMatches_();
    }

    this.pastedInInput_ = false;
  }

  private onInputKeydown_(e: KeyboardEvent) {
    // Ignore this event if the input does not have any inline autocompletion.
    if (!this.lastInput_.inline) {
      return;
    }

    const inputValue = this.$.input.value;
    const inputSelection = inputValue.substring(
        this.$.input.selectionStart!, this.$.input.selectionEnd!);
    const lastInputValue = this.lastInput_.text + this.lastInput_.inline;
    // If the current input state (its value and selection) matches its last
    // state (text and inline autocompletion) and the user types the next
    // character in the inline autocompletion, stop the keydown event. Just move
    // the selection and requery autocomplete. This is needed to avoid flicker.
    if (inputSelection === this.lastInput_.inline &&
        inputValue === lastInputValue &&
        this.lastInput_.inline[0].toLocaleLowerCase() ===
            e.key.toLocaleLowerCase()) {
      const text = this.lastInput_.text + e.key;
      assert(text);
      this.updateInput_({
        text: text,
        inline: this.lastInput_.inline.substr(1),
      });

      // If |charTypedTime_| is not 0, there's a pending typed character for
      // which the results have not been painted yet. In that case, keep the
      // earlier time.
      this.charTypedTime_ = this.charTypedTime_ || window.performance.now();

      // If 'CharTyped' mark already exists, there's a pending typed character
      // for which the results have not been painted yet. In that case, keep the
      // earlier mark.
      const metricsReporter = MetricsReporterImpl.getInstance();
      if (!metricsReporter.hasLocalMark('CharTyped')) {
        metricsReporter.mark('CharTyped');
      }

      this.queryAutocomplete_(this.lastInput_.text);
      e.preventDefault();
    }
  }

  private onInputKeyup_(e: KeyboardEvent) {
    if (e.key !== 'Tab') {
      return;
    }

    // Query for zero-prefix matches if user is tabbing into an empty input and
    // matches are not visible.
    if (!this.$.input.value && !this.matchesAreVisible) {
      this.queryAutocomplete_('');
    }
  }

  private onInputMouseDown_(e: MouseEvent) {
    if (e.button !== 0) {
      return;
    }

    // Query for zero-prefix matches when the main (generally left) mouse button
    // is pressed on an empty input and matches are not visible.
    if (!this.$.input.value && !this.matchesAreVisible) {
      this.queryAutocomplete_('');
    }
  }

  private onInputPaste_() {
    this.pastedInInput_ = true;
  }

  private onInputWrapperFocusout_(e: FocusEvent) {
    // Hide the matches and stop autocomplete only when the focus goes outside
    // of the realbox wrapper.
    if (!this.$.inputWrapper.contains(e.relatedTarget as Element)) {
      if (this.lastQueriedInput_ === '') {
        // Clear the input as well as the matches if the input was empty when
        // the matches arrived.
        this.updateInput_({text: '', inline: ''});
        this.clearAutocompleteMatches_();
      } else {
        this.matchesAreVisible = false;

        // Stop autocomplete but leave (potentially stale) results and continue
        // listening for key presses. These stale results should never be shown.
        // They correspond to the potentially stale suggestion left in the
        // realbox when blurred. That stale result may be navigated to by
        // focusing and pressing 'Enter'.
        this.pageHandler_.stopAutocomplete(/*clearResult=*/ false);
      }
    }
  }

  private onInputWrapperKeydown_(e: KeyboardEvent) {
    const KEYDOWN_HANDLED_KEYS = [
      'ArrowDown',
      'ArrowUp',
      'Delete',
      'Enter',
      'Escape',
      'PageDown',
      'PageUp',
    ];
    if (!KEYDOWN_HANDLED_KEYS.includes(e.key)) {
      return;
    }

    if (e.defaultPrevented) {
      // Ignore previously handled events.
      return;
    }

    // ArrowUp/ArrowDown query autocomplete when matches are not visible.
    if (!this.matchesAreVisible) {
      if (e.key === 'ArrowUp' || e.key === 'ArrowDown') {
        const inputValue = this.$.input.value;
        if (inputValue.trim() || !inputValue) {
          this.queryAutocomplete_(inputValue);
        }
        e.preventDefault();
        return;
      }
    }

    // Do not handle the following keys if there are no matches available.
    if (!this.result_ || this.result_.matches.length === 0) {
      return;
    }

    if (e.key === 'Delete') {
      if (e.shiftKey && !e.altKey && !e.ctrlKey && !e.metaKey) {
        if (this.selectedMatch_ && this.selectedMatch_.supportsDeletion) {
          this.pageHandler_.deleteAutocompleteMatch(this.selectedMatchIndex_);
          e.preventDefault();
        }
      }
      return;
    }

    // Do not handle the following keys if inside an IME composition session.
    if (e.isComposing) {
      return;
    }

    if (e.key === 'Enter') {
      const array: HTMLElement[] = [this.$.matches, this.$.input];
      if (array.includes(e.target as HTMLElement)) {
        if (this.lastQueriedInput_ !== null &&
            this.lastQueriedInput_.trimStart() ===
                decodeString16(this.result_.input)) {
          if (this.selectedMatch_) {
            this.navigateToMatch_(this.selectedMatchIndex_, e);
          }
        } else {
          // User typed and pressed 'Enter' too quickly. Ignore this for now
          // because the matches are stale. Navigate to the default match (if
          // one exists) once the up-to-date matches arrive.
          this.lastIgnoredEnterEvent_ = e;
          e.preventDefault();
        }
      }
      return;
    }

    // Do not handle the following keys if there are key modifiers.
    if (hasKeyModifiers(e)) {
      return;
    }

    // Clear the input as well as the matches when 'Escape' is pressed if the
    // the first match is selected or there are no selected matches.
    if (e.key === 'Escape' && this.selectedMatchIndex_ <= 0) {
      this.updateInput_({text: '', inline: ''});
      this.clearAutocompleteMatches_();
      e.preventDefault();
      return;
    }

    if (e.key === 'ArrowDown') {
      this.$.matches.selectNext();
      this.pageHandler_.onNavigationLikely(
          this.selectedMatchIndex_, NavigationPredictor.kUpOrDownArrowButton);
    } else if (e.key === 'ArrowUp') {
      this.$.matches.selectPrevious();
      this.pageHandler_.onNavigationLikely(
          this.selectedMatchIndex_, NavigationPredictor.kUpOrDownArrowButton);
    } else if (e.key === 'Escape' || e.key === 'PageUp') {
      this.$.matches.selectFirst();
    } else if (e.key === 'PageDown') {
      this.$.matches.selectLast();
    }
    e.preventDefault();

    // Focus the selected match if focus is currently in the matches.
    if (this.shadowRoot!.activeElement === this.$.matches) {
      this.$.matches.focusSelected();
    }

    // Update the input.
    const newFill = decodeString16(this.selectedMatch_!.fillIntoEdit);
    const newInline = this.selectedMatchIndex_ === 0 &&
            this.selectedMatch_!.allowedToBeDefaultMatch ?
        decodeString16(this.selectedMatch_!.inlineAutocompletion) :
        '';
    const newFillEnd = newFill.length - newInline.length;
    const text = newFill.substr(0, newFillEnd);
    assert(text);
    this.updateInput_({
      text: text,
      inline: newInline,
      moveCursorToEnd: newInline.length === 0,
    });
  }

  /**
   * @param e Event containing index of the match that was clicked.
   */
  private onMatchClick_(e: CustomEvent<{index: number, event: MouseEvent}>) {
    this.navigateToMatch_(e.detail.index, e.detail.event);
  }

  /**
   * @param e Event containing index of the match that received focus.
   */
  private onMatchFocusin_(e: CustomEvent<number>) {
    // Select the match that received focus.
    this.$.matches.selectIndex(e.detail);
    // Input selection (if any) likely drops due to focus change. Simply fill
    // the input with the match and move the cursor to the end.
    this.updateInput_({
      text: decodeString16(this.selectedMatch_!.fillIntoEdit),
      inline: '',
      moveCursorToEnd: true,
    });
  }

  /**
   * @param e Event containing index of the match that was removed.
   */
  private onMatchRemove_(e: CustomEvent<number>) {
    this.pageHandler_.deleteAutocompleteMatch(e.detail);
  }

  /**
   * @param e Event containing the result repaint time.
   */
  private onResultRepaint_(e: CustomEvent<number>) {
    if (this.charTypedTime_) {
      this.pageHandler_.logCharTypedToRepaintLatency(
          mojoTimeDelta(e.detail - this.charTypedTime_));
      this.charTypedTime_ = 0;
    }
  }

  private onVoiceSearchClick_() {
    this.dispatchEvent(new Event('open-voice-search'));
  }

  private onLensSearchClick_() {
    this.matchesAreVisible = false;
    this.dispatchEvent(new Event('open-lens-search'));
  }

  //============================================================================
  // Helpers
  //============================================================================

  private computeSelectedMatch_(): AutocompleteMatch|null {
    if (!this.result_ || !this.result_.matches) {
      return null;
    }
    return this.result_.matches[this.selectedMatchIndex_] || null;
  }

  /**
   * Clears the autocomplete result on the page and on the autocomplete backend.
   */
  private clearAutocompleteMatches_() {
    this.matchesAreVisible = false;
    this.result_ = null;
    this.$.matches.unselect();
    this.pageHandler_.stopAutocomplete(/*clearResult=*/ true);
    // Autocomplete sends updates once it is stopped. Invalidate those results
    // by setting the |this.lastQueriedInput_| to its default value.
    this.lastQueriedInput_ = null;
  }

  private navigateToMatch_(matchIndex: number, e: KeyboardEvent|MouseEvent) {
    assert(matchIndex >= 0);
    const match = this.result_!.matches[matchIndex];
    assert(match);
    assert(this.lastInputFocusTime_);
    const delta =
        mojoTimeDelta(window.performance.now() - this.lastInputFocusTime_);
    this.pageHandler_.openAutocompleteMatch(
        matchIndex, match.destinationUrl, this.matchesAreVisible, delta,
        (e as MouseEvent).button || 0, e.altKey, e.ctrlKey, e.metaKey,
        e.shiftKey);
    e.preventDefault();
  }

  private queryAutocomplete_(
      input: string, preventInlineAutocomplete: boolean = false) {
    this.lastQueriedInput_ = input;

    const caretNotAtEnd = this.$.input.selectionStart !== input.length;
    preventInlineAutocomplete = preventInlineAutocomplete ||
        this.isDeletingInput_ || this.pastedInInput_ || caretNotAtEnd;
    this.pageHandler_.queryAutocomplete(
        mojoString16(input), preventInlineAutocomplete);
  }

  /**
   * Updates the input state (text and inline autocompletion) with |update|.
   */
  private updateInput_(update: InputUpdate) {
    const newInput = Object.assign({}, this.lastInput_, update);
    const newInputValue = newInput.text + newInput.inline;
    const lastInputValue = this.lastInput_.text + this.lastInput_.inline;

    const inlineDiffers = newInput.inline !== this.lastInput_.inline;
    const preserveSelection = !inlineDiffers && !update.moveCursorToEnd;
    let needsSelectionUpdate = !preserveSelection;

    const oldSelectionStart = this.$.input.selectionStart;
    const oldSelectionEnd = this.$.input.selectionEnd;

    if (newInputValue !== this.$.input.value) {
      this.$.input.value = newInputValue;
      needsSelectionUpdate = true;  // Setting .value blows away selection.
    }

    if (newInputValue.trim() && needsSelectionUpdate) {
      // If the cursor is to be moved to the end (implies selection should not
      // be perserved), set the selection start to same as the selection end.
      this.$.input.selectionStart = preserveSelection ?
          oldSelectionStart :
          update.moveCursorToEnd ? newInputValue.length : newInput.text.length;
      this.$.input.selectionEnd =
          preserveSelection ? oldSelectionEnd : newInputValue.length;
    }

    this.isDeletingInput_ = lastInputValue.length > newInputValue.length &&
        lastInputValue.startsWith(newInputValue);
    this.lastInput_ = newInput;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ntp-realbox': RealboxElement;
  }
}

customElements.define(RealboxElement.is, RealboxElement);
