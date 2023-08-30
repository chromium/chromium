// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Generalized logic for providing spoken feedback when editing
 * text fields, both single and multiline fields.
 *
 * {@code ChromeVoxEditableTextBase} is a generalized class that takes the
 * current state in the form of a text string, a cursor start location and a
 * cursor end location, and calls a speak method with the resulting text to
 * be spoken.  This class can be used directly for single line fields or
 * extended to override methods that extract lines for multiline fields
 * or to provide other customizations.
 */
import {LocalStorage} from '../../../common/local_storage.js';
import {Msgs} from '../../common/msgs.js';
import {Personality, QueueMode, TtsCategory, TtsSpeechProperties} from '../../common/tts_types.js';
import {ChromeVoxState} from '../chromevox_state.js';
import {TtsInterface} from '../tts_interface.js';

import {TypingEcho} from './typing_echo.js';

/**
 * A class containing the information needed to speak
 * a text change event to the user.
 */
export class TextChangeEvent {
  /**
   * @param {string} newValue The new string value of the editable text control.
   * @param {number} newStart The new 0-based start cursor/selection index.
   * @param {number} newEnd The new 0-based end cursor/selection index.
   * @param {boolean} triggeredByUser .
   */
  constructor(newValue, newStart, newEnd, triggeredByUser) {
    /** @private {string} */
    this.value_ = '';
    this.value = newValue;

    this.start = newStart;
    this.end = newEnd;
    this.triggeredByUser = triggeredByUser;

    // Adjust offsets to be in left to right order.
    if (this.start > this.end) {
      const tempOffset = this.end;
      this.end = this.start;
      this.start = tempOffset;
    }
  }

  get value() {
    return this.value_;
  }
  set value(val) {
    this.value_ = val.replace(/\u00a0/g, ' ');
  }
}

/**
 * A class representing an abstracted editable text control.
 */
export class ChromeVoxEditableTextBase {
  /**
   * @param {string} value The string value of the editable text control.
   * @param {number} start The 0-based start cursor/selection index.
   * @param {number} end The 0-based end cursor/selection index.
   * @param {boolean} isPassword Whether the text control if a password field.
   * @param {TtsInterface} tts A TTS object.
   */
  constructor(value, start, end, isPassword, tts) {
    /**
     * Current value of the text field.
     * @type {string}
     * @private
     */
    this.value_ = '';
    Object.defineProperty(this, 'value', {
      get: () => this.value_,
      set: val => this.value_ = val.replace('\u00a0', ' '),
    });
    this.value = value;

    /**
     * 0-based selection start index.
     * @type {number}
     * @protected
     */
    this.start = start;

    /**
     * 0-based selection end index.
     * @type {number}
     * @protected
     */
    this.end = end;

    /**
     * True if this is a password field.
     * @type {boolean}
     * @protected
     */
    this.isPassword = isPassword;

    /**
     * Text-to-speech object implementing speak() and stop() methods.
     * @type {TtsInterface}
     * @protected
     */
    this.tts = tts;

    /**
     * Whether or not the text field is multiline.
     * @type {boolean}
     * @protected
     */
    this.multiline = false;

    /**
     * Whether or not the last update to the text and selection was described.
     *
     * Some consumers of this flag like |ChromeVoxEventWatcher| depend on and
     * react to when this flag is false by generating alternative feedback.
     * @type {boolean}
     */
    this.lastChangeDescribed = false;
  }

  /**
   * Performs setup for this element.
   */
  setup() {}

  /**
   * Performs teardown for this element.
   */
  teardown() {}

  /**
   * Get the line number corresponding to a particular index.
   * Default implementation that can be overridden by subclasses.
   * @param {number} index The 0-based character index.
   * @return {number} The 0-based line number corresponding to that character.
   */
  getLineIndex(index) {
    return 0;
  }

  /**
   * Get the start character index of a line.
   * Default implementation that can be overridden by subclasses.
   * @param {number} index The 0-based line index.
   * @return {number} The 0-based index of the first character in this line.
   */
  getLineStart(index) {
    return 0;
  }

  /**
   * Get the end character index of a line.
   * Default implementation that can be overridden by subclasses.
   * @param {number} index The 0-based line index.
   * @return {number} The 0-based index of the end of this line.
   */
  getLineEnd(index) {
    return this.value.length;
  }

  /**
   * Get the full text of the current line.
   * @param {number} index The 0-based line index.
   * @return {string} The text of the line.
   */
  getLine(index) {
    const lineStart = this.getLineStart(index);
    const lineEnd = this.getLineEnd(index);
    return this.value.substr(lineStart, lineEnd - lineStart);
  }

  /**
   * @param {string} ch The character to test.
   * @return {boolean} True if a character is whitespace.
   */
  isWhitespaceChar(ch) {
    return ch === ' ' || ch === '\n' || ch === '\r' || ch === '\t';
  }

  /**
   * @param {string} ch The character to test.
   * @return {boolean} True if a character breaks a word, used to determine
   *     if the previous word should be spoken.
   */
  isWordBreakChar(ch) {
    return Boolean(ch.match(/^\W$/));
  }

  /**
   * @param {TextChangeEvent} evt The new text changed event to test.
   * @return {boolean} True if the event, when compared to the previous text,
   * should trigger description.
   */
  shouldDescribeChange(evt) {
    if (evt.value === this.value && evt.start === this.start &&
        evt.end === this.end) {
      return false;
    }
    return true;
  }

  /**
   * Speak text, but if it's a single character, describe the character.
   * @param {string} str The string to speak.
   * @param {boolean=} opt_triggeredByUser True if the speech was triggered by a
   * user action.
   * @param {TtsSpeechProperties=} opt_personality Personality used to speak
   *     text.
   */
  speak(str, opt_triggeredByUser, opt_personality) {
    if (!str) {
      return;
    }
    let queueMode = QueueMode.QUEUE;
    if (opt_triggeredByUser === true) {
      queueMode = QueueMode.CATEGORY_FLUSH;
    }
    const props = opt_personality ?? new TtsSpeechProperties();
    props.category = TtsCategory.NAV;
    this.tts.speak(str, queueMode, props);
  }

  /**
   * Update the state of the text and selection and describe any changes as
   * appropriate.
   *
   * @param {TextChangeEvent} evt The text change event.
   */
  changed(evt) {
    if (!this.shouldDescribeChange(evt)) {
      this.lastChangeDescribed = false;
      return;
    }

    if (evt.value === this.value) {
      this.describeSelectionChanged(evt);
    } else {
      this.describeTextChanged(
          new TextChangeEvent(this.value, this.start, this.end, true), evt);
    }
    this.lastChangeDescribed = true;

    this.value = evt.value;
    this.start = evt.start;
    this.end = evt.end;
  }

  /**
   * Describe a change in the selection or cursor position when the text
   * stays the same.
   * @param {TextChangeEvent} evt The text change event.
   */
  describeSelectionChanged(evt) {
    // TODO(deboer): Factor this into two function:
    //   - one to determine the selection event
    //   - one to speak

    if (this.isPassword) {
      this.speak(
          (new goog.i18n.MessageFormat(Msgs.getMsg('bullet')).format({
            'COUNT': 1,
          })),
          evt.triggeredByUser);
      return;
    }
    if (evt.start === evt.end) {
      // It's currently a cursor.
      if (this.start !== this.end) {
        // It was previously a selection.
        this.speak(
            this.value.substring(this.start, this.end), evt.triggeredByUser);
        this.speak(Msgs.getMsg('removed_from_selection'));
      } else if (
          this.getLineIndex(this.start) !== this.getLineIndex(evt.start)) {
        // Moved to a different line; read it.
        let lineValue = this.getLine(this.getLineIndex(evt.start));
        if (lineValue === '') {
          lineValue = Msgs.getMsg('text_box_blank');
        } else if (lineValue === '\n') {
          // Pass through the literal line value so character outputs 'new
          // line'.
        } else if (/^\s+$/.test(lineValue)) {
          lineValue = Msgs.getMsg('text_box_whitespace');
        }
        this.speak(lineValue, evt.triggeredByUser);
      } else if (this.start === evt.start + 1 || this.start === evt.start - 1) {
        // Moved by one character; read it.
        if (evt.start === this.value.length) {
          this.speak(Msgs.getMsg('end_of_text_verbose'), evt.triggeredByUser);
        } else {
          this.speak(
              this.value.substr(evt.start, 1), evt.triggeredByUser,
              new TtsSpeechProperties(
                  {'phoneticCharacters': evt.triggeredByUser}));
        }
      } else {
        // Moved by more than one character. Read all characters crossed.
        this.speak(
            this.value.substr(
                Math.min(this.start, evt.start),
                Math.abs(this.start - evt.start)),
            evt.triggeredByUser);
      }
    } else {
      // It's currently a selection.
      if (this.start + 1 === evt.start && this.end === this.value.length &&
          evt.end === this.value.length) {
        // Autocomplete: the user typed one character of autocompleted text.
        if (LocalStorage.get('typingEcho') === TypingEcho.CHARACTER ||
            LocalStorage.get('typingEcho') === TypingEcho.CHARACTER_AND_WORD) {
          this.speak(this.value.substr(this.start, 1), evt.triggeredByUser);
        }
        this.speak(this.value.substr(evt.start));
      } else if (this.start === this.end) {
        // It was previously a cursor.
        this.speak(
            this.value.substr(evt.start, evt.end - evt.start),
            evt.triggeredByUser);
        this.speak(Msgs.getMsg('selected'));
      } else if (this.start === evt.start && this.end < evt.end) {
        this.speak(
            this.value.substr(this.end, evt.end - this.end),
            evt.triggeredByUser);
        this.speak(Msgs.getMsg('added_to_selection'));
      } else if (this.start === evt.start && this.end > evt.end) {
        this.speak(
            this.value.substr(evt.end, this.end - evt.end),
            evt.triggeredByUser);
        this.speak(Msgs.getMsg('removed_from_selection'));
      } else if (this.end === evt.end && this.start > evt.start) {
        this.speak(
            this.value.substr(evt.start, this.start - evt.start),
            evt.triggeredByUser);
        this.speak(Msgs.getMsg('added_to_selection'));
      } else if (this.end === evt.end && this.start < evt.start) {
        this.speak(
            this.value.substr(this.start, evt.start - this.start),
            evt.triggeredByUser);
        this.speak(Msgs.getMsg('removed_from_selection'));
      } else {
        // The selection changed but it wasn't an obvious extension of
        // a previous selection. Just read the new selection.
        this.speak(
            this.value.substr(evt.start, evt.end - evt.start),
            evt.triggeredByUser);
        this.speak(Msgs.getMsg('selected'));
      }
    }
  }

  /**
   * Describe a change where the text changes.
   * @param {TextChangeEvent} prev The previous text change event.
   * @param {TextChangeEvent} evt The text change event.
   */
  describeTextChanged(prev, evt) {
    let personality = new TtsSpeechProperties();
    if (evt.value.length < (prev.value.length - 1)) {
      personality = Personality.DELETED;
    }
    if (this.isPassword) {
      this.speak(
          (new goog.i18n.MessageFormat(Msgs.getMsg('bullet')).format({
            'COUNT': 1,
          })),
          evt.triggeredByUser, personality);
      return;
    }

    const value = prev.value;
    const len = value.length;
    const newLen = evt.value.length;
    let autocompleteSuffix = '';
    // Make a copy of evtValue and evtEnd to avoid changing anything in
    // the event itself.
    let evtValue = evt.value;
    let evtEnd = evt.end;

    // First, see if there's a selection at the end that might have been
    // added by autocomplete. If so, strip it off into a separate variable.
    if (evt.start < evtEnd && evtEnd === newLen) {
      autocompleteSuffix = evtValue.substr(evt.start);
      evtValue = evtValue.substr(0, evt.start);
      evtEnd = evt.start;
    }

    // Now see if the previous selection (if any) was deleted
    // and any new text was inserted at that character position.
    // This would handle pasting and entering text by typing, both from
    // a cursor and from a selection.
    let prefixLen = prev.start;
    let suffixLen = len - prev.end;
    if (newLen >= prefixLen + suffixLen + (evtEnd - evt.start) &&
        evtValue.substr(0, prefixLen) === value.substr(0, prefixLen) &&
        evtValue.substr(newLen - suffixLen) === value.substr(prev.end)) {
      this.describeTextChangedHelper(
          prev, evt, prefixLen, suffixLen, autocompleteSuffix, personality);
      return;
    }

    // Next, see if one or more characters were deleted from the previous
    // cursor position and the new cursor is in the expected place. This
    // handles backspace, forward-delete, and similar shortcuts that delete
    // a word or line.
    prefixLen = evt.start;
    suffixLen = newLen - evtEnd;
    if (prev.start === prev.end && evt.start === evtEnd &&
        evtValue.substr(0, prefixLen) === value.substr(0, prefixLen) &&
        evtValue.substr(newLen - suffixLen) === value.substr(len - suffixLen)) {
      // Forward deletions causes reading of the character immediately to the
      // right of the caret or the deleted text depending on the iBeam cursor
      // setting.
      if (prev.start === evt.start && prev.end === evt.end) {
        this.speak(evt.value[evt.start], evt.triggeredByUser);
      } else {
        this.describeTextChangedHelper(
            prev, evt, prefixLen, suffixLen, autocompleteSuffix, personality);
      }
      return;
    }

    // If all else fails, we assume the change was not the result of a normal
    // user editing operation, so we'll have to speak feedback based only
    // on the changes to the text, not the cursor position / selection.
    // First, restore the autocomplete text if any.
    evtValue += autocompleteSuffix;

    // Try to do a diff between the new and the old text. If it is a one
    // character insertion/deletion at the start or at the end, just speak that
    // character.
    if ((evtValue.length === (value.length + 1)) ||
        ((evtValue.length + 1) === value.length)) {
      // The user added text either to the beginning or the end.
      if (evtValue.length > value.length) {
        if (evtValue.startsWith(value)) {
          this.speak(
              evtValue[evtValue.length - 1], evt.triggeredByUser, personality);
          return;
        } else if (evtValue.indexOf(value) === 1) {
          this.speak(evtValue[0], evt.triggeredByUser, personality);
          return;
        }
      }
      // The user deleted text either from the beginning or the end.
      if (evtValue.length < value.length) {
        if (value.startsWith(evtValue)) {
          this.speak(value[value.length - 1], evt.triggeredByUser, personality);
          return;
        } else if (value.indexOf(evtValue) === 1) {
          this.speak(value[0], evt.triggeredByUser, personality);
          return;
        }
      }
    }

    if (this.multiline) {
      // The below is a somewhat loose way to deal with non-standard
      // insertions/deletions. Intentionally skip for multiline since deletion
      // announcements are covered above and insertions are non-standard
      // (possibly due to auto complete). Since content editable's often refresh
      // content by removing and inserting entire chunks of text, this type of
      // logic often results in unintended consequences such as reading all text
      // when only one character has been entered.
      return;
    }

    // If the text is short, just speak the whole thing.
    if (newLen <= this.maxShortPhraseLen) {
      this.describeTextChangedHelper(prev, evt, 0, 0, '', personality);
      return;
    }

    // Otherwise, look for the common prefix and suffix, but back up so
    // that we can speak complete words, to be minimally confusing.
    prefixLen = 0;
    while (prefixLen < len && prefixLen < newLen &&
           value[prefixLen] === evtValue[prefixLen]) {
      prefixLen++;
    }
    while (prefixLen > 0 && !this.isWordBreakChar(value[prefixLen - 1])) {
      prefixLen--;
    }

    suffixLen = 0;
    while (suffixLen < (len - prefixLen) && suffixLen < (newLen - prefixLen) &&
           value[len - suffixLen - 1] === evtValue[newLen - suffixLen - 1]) {
      suffixLen++;
    }
    while (suffixLen > 0 && !this.isWordBreakChar(value[len - suffixLen])) {
      suffixLen--;
    }

    this.describeTextChangedHelper(
        prev, evt, prefixLen, suffixLen, '', personality);
  }

  /**
   * The function called by describeTextChanged after it's figured out
   * what text was deleted, what text was inserted, and what additional
   * autocomplete text was added.
   * @param {TextChangeEvent} prev The previous text change event.
   * @param {TextChangeEvent} evt The text change event.
   * @param {number} prefixLen The number of characters in the common prefix
   *     of this.value and newValue.
   * @param {number} suffixLen The number of characters in the common suffix
   *     of this.value and newValue.
   * @param {string} autocompleteSuffix The autocomplete string that was added
   *     to the end, if any. It should be spoken at the end of the utterance
   *     describing the change.
   * @param {TtsSpeechProperties=} opt_personality Personality to speak the
   *     text.
   */
  describeTextChangedHelper(
      prev, evt, prefixLen, suffixLen, autocompleteSuffix, opt_personality) {
    const len = prev.value.length;
    const newLen = evt.value.length;
    const deletedLen = len - prefixLen - suffixLen;
    const deleted = prev.value.substr(prefixLen, deletedLen);
    const insertedLen = newLen - prefixLen - suffixLen;
    const inserted = evt.value.substr(prefixLen, insertedLen);
    let utterance = '';
    let triggeredByUser = evt.triggeredByUser;

    if (insertedLen > 1) {
      if (!ChromeVoxEditableTextBase.shouldSpeakInsertions) {
        return;
      }
      utterance = inserted;
    } else if (insertedLen === 1) {
      if ((LocalStorage.get('typingEcho') === TypingEcho.WORD ||
           LocalStorage.get('typingEcho') === TypingEcho.CHARACTER_AND_WORD) &&
          this.isWordBreakChar(inserted) && prefixLen > 0 &&
          !this.isWordBreakChar(evt.value.substr(prefixLen - 1, 1))) {
        // Speak previous word.
        let index = prefixLen;
        while (index > 0 && !this.isWordBreakChar(evt.value[index - 1])) {
          index--;
        }
        if (index < prefixLen) {
          utterance = evt.value.substr(index, prefixLen + 1 - index);
        } else {
          utterance = inserted;
          triggeredByUser = false;  // Implies QUEUE_MODE_QUEUE.
        }
      } else if (
          LocalStorage.get('typingEcho') === TypingEcho.CHARACTER ||
          LocalStorage.get('typingEcho') === TypingEcho.CHARACTER_AND_WORD) {
        utterance = inserted;
      }
    } else if (deletedLen > 1 && !autocompleteSuffix) {
      utterance = deleted + ', deleted';
    } else if (deletedLen === 1) {
      utterance = deleted;
      // Single-deleted characters should also use Personality.DELETED.
      opt_personality = Personality.DELETED;
    }

    if (autocompleteSuffix && utterance) {
      utterance += ', ' + autocompleteSuffix;
    } else if (autocompleteSuffix) {
      utterance = autocompleteSuffix;
    }

    if (utterance) {
      this.speak(utterance, triggeredByUser, opt_personality);
    }
  }

  /**
   * Moves the cursor forward by one character.
   * @return {boolean} True if the action was handled.
   */
  moveCursorToNextCharacter() {
    return false;
  }

  /**
   * Moves the cursor backward by one character.
   * @return {boolean} True if the action was handled.
   */
  moveCursorToPreviousCharacter() {
    return false;
  }

  /**
   * Moves the cursor forward by one word.
   * @return {boolean} True if the action was handled.
   */
  moveCursorToNextWord() {
    return false;
  }

  /**
   * Moves the cursor backward by one word.
   * @return {boolean} True if the action was handled.
   */
  moveCursorToPreviousWord() {
    return false;
  }

  /**
   * Moves the cursor forward by one line.
   * @return {boolean} True if the action was handled.
   */
  moveCursorToNextLine() {
    return false;
  }

  /**
   * Moves the cursor backward by one line.
   * @return {boolean} True if the action was handled.
   */
  moveCursorToPreviousLine() {
    return false;
  }

  /**
   * Moves the cursor forward by one paragraph.
   * @return {boolean} True if the action was handled.
   */
  moveCursorToNextParagraph() {
    return false;
  }

  /**
   * Moves the cursor backward by one paragraph.
   * @return {boolean} True if the action was handled.
   */
  moveCursorToPreviousParagraph() {
    return false;
  }
}

/**
 * @type {boolean} Whether insertions (i.e. changes of greater than one
 * character) should be spoken.
 */
ChromeVoxEditableTextBase.shouldSpeakInsertions = false;

/**
 * The maximum number of characters that are short enough to speak in response
 * to an event. For example, if the user selects "Hello", we will speak
 * "Hello, selected", but if the user selects 1000 characters, we will speak
 * "text selected" instead.
 *
 * @type {number}
 */
ChromeVoxEditableTextBase.prototype.maxShortPhraseLen = 60;
