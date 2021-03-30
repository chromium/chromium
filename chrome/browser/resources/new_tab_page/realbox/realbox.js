// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './realbox_dropdown.js';
import './realbox_icon.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {skColorToRgba} from 'chrome://resources/js/color_utils.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {decodeString16, mojoString16, mojoTimeDelta} from '../utils.js';

import {RealboxBrowserProxy} from './realbox_browser_proxy.js';

/** @typedef {{text: string, inline: string,}} */
let Input;

/**
 * @typedef {{
 *   text: (string|undefined),
 *   inline: (string|undefined),
 *   moveCursorToEnd: (boolean|undefined),
 * }}
 */
let InputUpdate;

// A real search box that behaves just like the Omnibox.
class RealboxElement extends PolymerElement {
  static get is() {
    return 'ntp-realbox';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      //========================================================================
      // Public properties
      //========================================================================

      /**
       * Whether matches are currently visible.
       * @type {boolean}
       */
      matchesAreVisible: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      /**
       * @type {!realbox.mojom.SearchBoxTheme}
       */
      theme: {
        type: Object,
        observer: 'onThemeChange_',
      },

      //========================================================================
      // Private properties
      //========================================================================

      /**
       * The time of the first character insert operation that has not yet been
       * painted in milliseconds. Used to measure the realbox responsiveness.
       * @type {number}
       * @private
       */
      charTypedTime_: {
        type: Number,
        value: 0,
      },

      /**
       * Whether user is deleting text in the input. Used to prevent the default
       * match from offering inline autocompletion.
       * @type {boolean}
       * @private
       */
      isDeletingInput_: {
        type: Boolean,
        value: false,
      },

      /**
       * The 'Enter' keydown event that was ignored due to matches being stale.
       * Used to navigate to the default match once up-to-date matches arrive.
       * @type {?Event}
       * @private
       */
      lastIgnoredEnterEvent_: {
        type: Object,
        value: null,
      },

      /**
       * Last state of the input (text and inline autocompletion). Updated
       * by the user input or by the currently selected autocomplete match.
       * @type {!Input}
       * @private
       */
      lastInput_: {
        type: Object,
        value: {text: '', inline: ''},
      },

      /**
       * The time at which the input was last focused in milliseconds. Passed to
       * the browser when navigating to a match.
       * @type {?number}
       * @private
       */
      lastInputFocusTime_: {
        type: Number,
        value: null,
      },

      /**
       * The last queried input text.
       * @type {?string}
       * @private
       */
      lastQueriedInput_: {
        type: String,
        value: null,
      },

      /**
       * True if user just pasted into the input. Used to prevent the default
       * match from offering inline autocompletion.
       * @type {boolean}
       * @private
       */
      pastedInInput_: {
        type: Boolean,
        value: false,
      },

      /**
       * Realbox default icon (i.e., Google G icon or the search loupe).
       * @private
       */
      realboxIcon_: {
        type: String,
        value: () => loadTimeData.getString('realboxDefaultIcon'),
      },

      /**
       * @type {?search.mojom.AutocompleteResult}
       * @private
       */
      result_: {
        type: Object,
      },

      /**
       * The currently selected match, if any.
       * @type {?search.mojom.AutocompleteMatch}
       * @private
       */
      selectedMatch_: {
        type: Object,
        computed: `computeSelectedMatch_(result_, selectedMatchIndex_)`,
      },

      /**
       * Index of the currently selected match, if any.
       * Do not modify this. Use <ntp-realbox-dropdown> API to change selection.
       * @type {number}
       * @private
       */
      selectedMatchIndex_: {
        type: Number,
        value: -1,
      },

      /**
       * The value of the input element's 'aria-live' attribute.
       * @type {string}
       * @private
       */
      inputAriaLive_: {
        type: String,
        computed: `computeInputAriaLive_(selectedMatch_)`,
      },
    };
  }

  computeInputAriaLive_() {
    return this.selectedMatch_ ? 'off' : 'polite';
  }

  constructor() {
    performance.mark('realbox-creation-start');
    super();
    /** @private {realbox.mojom.PageHandlerRemote} */
    this.pageHandler_ = RealboxBrowserProxy.getInstance().handler;
    /** @private {!realbox.mojom.PageCallbackRouter} */
    this.callbackRouter_ = RealboxBrowserProxy.getInstance().callbackRouter;
    /** @private {?number} */
    this.autocompleteResultChangedListenerId_ = null;
    /** @private {?number} */
    this.autocompleteMatchImageAvailableListenerId_ = null;
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();
    this.autocompleteResultChangedListenerId_ =
        this.callbackRouter_.autocompleteResultChanged.addListener(
            this.onAutocompleteResultChanged_.bind(this));
    this.autocompleteMatchImageAvailableListenerId_ =
        this.callbackRouter_.autocompleteMatchImageAvailable.addListener(
            this.onAutocompleteMatchImageAvailable_.bind(this));
  }

  /** @override */
  disconnectedCallback() {
    super.disconnectedCallback();
    this.callbackRouter_.removeListener(
        assert(this.autocompleteResultChangedListenerId_));
    this.callbackRouter_.removeListener(
        assert(this.autocompleteMatchImageAvailableListenerId_));
  }

  /** @override */
  ready() {
    super.ready();
    performance.measure('realbox-creation', 'realbox-creation-start');
  }

  //============================================================================
  // Callbacks
  //============================================================================

  /**
   * @param {number} matchIndex match index
   * @param {!url.mojom.Url} url match imageUrl or destinationUrl.
   * @param {string} dataUrl match image or favicon content in in base64 encoded
   *     Data URL format.
   * @private
   */
  onAutocompleteMatchImageAvailable_(matchIndex, url, dataUrl) {
    if (!this.result_ || !this.result_.matches) {
      return;
    }

    const match = this.result_.matches[matchIndex];
    if (!match || this.selectedMatchIndex_ !== matchIndex) {
      return;
    }

    // Set favicon content of the selected match, if applicable.
    if (match.destinationUrl.url === url.url) {
      /** @suppress {checkTypes} */
      match.faviconDataUrl = dataUrl;
      this.notifyPath('selectedMatch_.faviconDataUrl');
    }
  }

  /**
   * @private
   * @param {search.mojom.AutocompleteResult} result
   */
  onAutocompleteResultChanged_(result) {
    if (this.lastQueriedInput_ === null ||
        this.lastQueriedInput_.trimLeft() !== decodeString16(result.input)) {
      return;  // Stale result; ignore.
    }

    this.result_ = result;
    const hasMatches = result && result.matches && result.matches.length > 0;
    this.matchesAreVisible = hasMatches;

    this.$.input.focus();

    const firstMatch = hasMatches ? this.result_.matches[0] : null;
    if (firstMatch && firstMatch.allowedToBeDefaultMatch) {
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
      // Restore the selection, if any.
      this.$.matches.selectIndex(this.selectedMatchIndex_);
      this.updateInput_({
        text: decodeString16(this.selectedMatch_.fillIntoEdit),
        inline: '',
        moveCursorToEnd: true
      });
    } else {
      this.$.matches.unselect();
      this.updateInput_({
        inline: '',
      });
    }
  }

  /**
   * @private
   */
  onThemeChange_() {
    if (!loadTimeData.getBoolean('realboxMatchOmniboxTheme')) {
      return;
    }

    this.updateStyles({
      '--search-box-bg': skColorToRgba(assert(this.theme.bg)),
      '--search-box-placeholder': skColorToRgba(assert(this.theme.placeholder)),
      '--search-box-results-bg': skColorToRgba(assert(this.theme.resultsBg)),
      '--search-box-text': skColorToRgba(assert(this.theme.text)),
      '--search-box-icon': skColorToRgba(assert(this.theme.icon)),
    });
  }

  //============================================================================
  // Event handlers
  //============================================================================

  /**
   * @private
   */
  onHeaderFocusin_() {
    // The header got focus. Unselect the selected match and clear the input.
    assert(this.lastQueriedInput_ === '');
    this.$.matches.unselect();
    this.updateInput_({text: '', inline: ''});
  }

  /**
   * @param {!Event} e
   * @private
   */
  onInputCutCopy_(e) {
    // Only handle cut/copy when input has content and it's all selected.
    if (!this.$.input.value || this.$.input.selectionStart !== 0 ||
        this.$.input.selectionEnd !== this.$.input.value.length ||
        !this.result_ || this.result_.matches.length === 0) {
      return;
    }

    if (this.selectedMatch_ && !this.selectedMatch_.isSearchType) {
      e.clipboardData.setData(
          'text/plain', this.selectedMatch_.destinationUrl.url);
      e.preventDefault();
      if (e.type === 'cut') {
        this.$.input.value = '';
      }
    }
  }

  /**
   * @param {!FocusEvent} e
   * @private
   */
  onInputFocus_(e) {
    this.lastInputFocusTime_ = window.performance.now();
    e.target.placeholder = '';
  }

  /**
   * @param {!FocusEvent} e
   * @private
   */
  onInputBlur_(e) {
    e.target.placeholder = loadTimeData.getString('realboxHint');
  }

  /**
   * @param {!InputEvent} e
   * @suppress {missingProperties} 'isComposing' is not defined on InputEvent.
   * @private
   */
  onInputInput_(e) {
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

  /**
   * @param {Event} e
   * @private
   */
  onInputKeydown_(e) {
    // Ignore this event if the input does not have any inline autocompletion.
    if (!this.lastInput_.inline) {
      return;
    }

    const inputValue = this.$.input.value;
    const inputSelection = inputValue.substring(
        this.$.input.selectionStart, this.$.input.selectionEnd);
    const lastInputValue = this.lastInput_.text + this.lastInput_.inline;
    // If the current input state (its value and selection) matches its last
    // state (text and inline autocompletion) and the user types the next
    // character in the inline autocompletion, stop the keydown event. Just move
    // the selection and requery autocomplete. This is needed to avoid flicker.
    if (inputSelection === this.lastInput_.inline &&
        inputValue === lastInputValue &&
        this.lastInput_.inline[0].toLocaleLowerCase() ===
            e.key.toLocaleLowerCase()) {
      this.updateInput_({
        text: assert(this.lastInput_.text + e.key),
        inline: this.lastInput_.inline.substr(1),
      });

      // If |charTypedTime_| is not 0, there's a pending typed character for
      // which the results have not been painted yet. In that case, keep the
      // earlier time.
      this.charTypedTime_ = this.charTypedTime_ || window.performance.now();

      this.queryAutocomplete_(this.lastInput_.text);
      e.preventDefault();
    }
  }

  /**
   * @param {Event} e
   * @private
   */
  onInputKeyup_(e) {
    if (e.key !== 'Tab') {
      return;
    }

    // Query for zero-prefix matches if user is tabbing into an empty input.
    if (!this.$.input.value) {
      this.queryAutocomplete_('');
    }
  }

  /**
   * @param {Event} e
   * @private
   */
  onInputMouseDown_(e) {
    if (e.button !== 0) {
      // Only handle main (generally left) button presses.
      return;
    }
    if (!this.$.input.value) {
      this.queryAutocomplete_('');
    }
  }

  /**
   * @param {Event} e
   * @private
   */
  onInputPaste_(e) {
    this.pastedInInput_ = true;
  }

  /**
   * @param {Event} e
   * @private
   */
  onInputWrapperFocusout_(e) {
    // Hide the matches and stop autocomplete only when the focus goes outside
    // of the realbox wrapper.
    const relatedTarget = /** @type {Element} */ (e.relatedTarget);
    if (!this.$.inputWrapper.contains(relatedTarget)) {
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

  /**
   * @param {Event} e
   * @private
   */
  onInputWrapperKeydown_(e) {
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
      // Ignore previousely handled events.
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
      if ([this.$.matches, this.$.input].includes(e.target)) {
        if (this.lastQueriedInput_ !== null &&
            this.lastQueriedInput_.trimLeft() ===
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
    if (e.altKey || e.ctrlKey || e.metaKey || e.shiftKey) {
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
    } else if (e.key === 'ArrowUp') {
      this.$.matches.selectPrevious();
    } else if (e.key === 'Escape' || e.key === 'PageUp') {
      this.$.matches.selectFirst();
    } else if (e.key === 'PageDown') {
      this.$.matches.selectLast();
    }
    e.preventDefault();

    // Focus the selected match if focus is currently in the matches.
    if (this.shadowRoot.activeElement === this.$.matches) {
      this.$.matches.focusSelected();
    }

    // Update the input.
    const newFill = decodeString16(this.selectedMatch_.fillIntoEdit);
    const newInline = this.selectedMatch_.allowedToBeDefaultMatch ?
        decodeString16(this.selectedMatch_.inlineAutocompletion) :
        '';
    const newFillEnd = newFill.length - newInline.length;
    this.updateInput_({
      text: assert(newFill.substr(0, newFillEnd)),
      inline: newInline,
      moveCursorToEnd: newInline.length === 0,
    });
  }

  /**
   * @param {!CustomEvent<{index: number, event:!MouseEvent}>} e Event
   *     containing index of the match that was clicked.
   * @private
   */
  onMatchClick_(e) {
    this.navigateToMatch_(e.detail.index, e.detail.event);
  }

  /**
   * @param {!CustomEvent<number>} e Event containing index of the match that
   *     received focus.
   * @private
   */
  onMatchFocusin_(e) {
    // Select the match that received focus.
    this.$.matches.selectIndex(e.detail);
    // Input selection (if any) likely drops due to focus change. Simply fill
    // the input with the match and move the cursor to the end.
    this.updateInput_({
      text: decodeString16(this.selectedMatch_.fillIntoEdit),
      inline: '',
      moveCursorToEnd: true
    });
  }

  /**
   * @param {!CustomEvent<number>} e Event containing index of the match that
   *     was removed.
   * @private
   */
  onMatchRemove_(e) {
    this.pageHandler_.deleteAutocompleteMatch(e.detail);
  }

  /**
   * @param {!CustomEvent<number>} e Event containing the result repaint time.
   * @private
   */
  onResultRepaint_(e) {
    if (this.charTypedTime_) {
      this.pageHandler_.logCharTypedToRepaintLatency(
          mojoTimeDelta(e.detail - this.charTypedTime_));
      this.charTypedTime_ = 0;
    }
  }

  /** @private */
  onVoiceSearchClick_() {
    this.dispatchEvent(new Event('open-voice-search'));
  }

  //============================================================================
  // Helpers
  //============================================================================

  /**
   * @return {?search.mojom.AutocompleteMatch}
   * @private
   */
  computeSelectedMatch_() {
    if (!this.result_ || !this.result_.matches) {
      return null;
    }
    return this.result_.matches[this.selectedMatchIndex_] || null;
  }

  /**
   * Clears the autocomplete result on the page and on the autocomplete backend.
   * @private
   */
  clearAutocompleteMatches_() {
    this.matchesAreVisible = false;
    this.result_ = null;
    this.$.matches.unselect();
    this.pageHandler_.stopAutocomplete(/*clearResult=*/ true);
    // Autocomplete sends updates once it is stopped. Invalidate those results
    // by setting the |this.lastQueriedInput_| to its default value.
    this.lastQueriedInput_ = null;
  }

  /**
   * @param {number} matchIndex
   * @param {!Event} e
   * @private
   */
  navigateToMatch_(matchIndex, e) {
    assert(matchIndex >= 0);
    const match = assert(this.result_.matches[matchIndex]);
    assert(this.lastInputFocusTime_);
    const delta =
        mojoTimeDelta(window.performance.now() - this.lastInputFocusTime_);
    this.pageHandler_.openAutocompleteMatch(
        matchIndex, match.destinationUrl, this.matchesAreVisible, delta,
        e.button || 0, e.altKey, e.ctrlKey, e.metaKey, e.shiftKey);
    e.preventDefault();
  }

  /**
   * @param {string} input
   * @param {boolean} preventInlineAutocomplete
   * @private
   */
  queryAutocomplete_(input, preventInlineAutocomplete = false) {
    this.lastQueriedInput_ = input;

    const caretNotAtEnd = this.$.input.selectionStart !== input.length;
    preventInlineAutocomplete = preventInlineAutocomplete ||
        this.isDeletingInput_ || this.pastedInInput_ || caretNotAtEnd;
    this.pageHandler_.queryAutocomplete(
        mojoString16(input), preventInlineAutocomplete);
  }

  /**
   * Updates the input state (text and inline autocompletion) with |update|.
   * @param {!InputUpdate} update
   * @private
   */
  updateInput_(update) {
    const newInput =
        /** @type {!Input} */ (Object.assign({}, this.lastInput_, update));
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

customElements.define(RealboxElement.is, RealboxElement);
