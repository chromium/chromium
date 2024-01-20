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
import {StringUtil} from '../../../common/string_util.js';
import {Msgs} from '../../common/msgs.js';
import {Personality, QueueMode, TtsCategory, TtsSpeechProperties} from '../../common/tts_types.js';
import {TtsInterface} from '../tts_interface.js';

import {TypingEchoState} from './typing_echo.js';

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
   * @param {number} charIndex
   * @return {number}
   */
  getLineIndex(charIndex) {
    return 0;
  }
  /**
   * @param {number} lineIndex
   * @return {number}
   */
  getLineStart(lineIndex) {
    return 0;
  }
  /**
   * @param {number} lineIndex
   * @return {number}
   */
  getLineEnd(lineIndex) {
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
      this.speak(Msgs.getMsg('password_char'), evt.triggeredByUser);
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
        if (LocalStorage.get('typingEcho') === TypingEchoState.CHARACTER ||
            LocalStorage.get('typingEcho') ===
                TypingEchoState.CHARACTER_AND_WORD) {
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
          Msgs.getMsg('password_char'), evt.triggeredByUser, personality);
      return;
    }

    // First, see if there's a selection at the end that might have been
    // added by autocomplete. If so, replace the event information with it.
    const origEvt = evt;
    let autocompleteSuffix = '';
    if (evt.start < evt.end && evt.end === evt.value.length) {
      autocompleteSuffix = evt.value.slice(evt.start);
      evt = new TextChangeEvent(
          evt.value.slice(0, evt.start), evt.start, evt.start,
          evt.triggeredByUser);
    }

    // Precompute the length of prefix and suffix of values.
    const commonPrefixLen =
        StringUtil.longestCommonPrefixLength(evt.value, prev.value);
    const commonSuffixLen =
        StringUtil.longestCommonSuffixLength(evt.value, prev.value);

    // Now see if the previous selection (if any) was deleted
    // and any new text was inserted at that character position.
    // This would handle pasting and entering text by typing, both from
    // a cursor and from a selection.
    let prefixLen = prev.start;
    let suffixLen = prev.value.length - prev.end;
    if (evt.value.length >= prefixLen + suffixLen + (evt.end - evt.start) &&
        commonPrefixLen >= prefixLen && commonSuffixLen >= suffixLen) {
      this.describeTextChangedHelper(
          prev, origEvt, prefixLen, suffixLen, autocompleteSuffix, personality);
      return;
    }

    // Next, see if one or more characters were deleted from the previous
    // cursor position and the new cursor is in the expected place. This
    // handles backspace, forward-delete, and similar shortcuts that delete
    // a word or line.
    prefixLen = evt.start;
    suffixLen = evt.value.length - evt.end;
    if (prev.start === prev.end && evt.start === evt.end &&
        commonPrefixLen >= prefixLen && commonSuffixLen >= suffixLen) {
      // Forward deletions causes reading of the character immediately to the
      // right of the caret.
      if (prev.start === evt.start && prev.end === evt.end) {
        this.speak(evt.value[evt.start], evt.triggeredByUser);
      } else {
        this.describeTextChangedHelper(
            prev, origEvt, prefixLen, suffixLen, autocompleteSuffix,
            personality);
      }
      return;
    }

    // See if the change is related to IME's complex operation.
    if (this.describeTextChangedByIME(
            prev, evt, commonPrefixLen, commonSuffixLen)) {
      return;
    }

    // If all above fails, we assume the change was not the result of a normal
    // user editing operation, so we'll have to speak feedback based only
    // on the changes to the text, not the cursor position / selection.
    // First, restore the event.
    evt = origEvt;

    // Try to do a diff between the new and the old text. If it is a one
    // character insertion/deletion at the start or at the end, just speak that
    // character.
    if ((evt.value.length === (prev.value.length + 1)) ||
        ((evt.value.length + 1) === prev.value.length)) {
      // The user added text either to the beginning or the end.
      if (evt.value.length > prev.value.length) {
        if (commonPrefixLen === prev.value.length) {
          this.speak(
              evt.value[evt.value.length - 1], evt.triggeredByUser,
              personality);
          return;
        } else if (commonSuffixLen === prev.value.length) {
          this.speak(evt.value[0], evt.triggeredByUser, personality);
          return;
        }
      }
      // The user deleted text either from the beginning or the end.
      if (evt.value.length < prev.value.length) {
        if (commonPrefixLen === evt.value.length) {
          this.speak(
              prev.value[prev.value.length - 1], evt.triggeredByUser,
              personality);
          return;
        } else if (commonSuffixLen === evt.value.length) {
          this.speak(prev.value[0], evt.triggeredByUser, personality);
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
    if (evt.value.length <= this.maxShortPhraseLen) {
      this.describeTextChangedHelper(prev, evt, 0, 0, '', personality);
      return;
    }

    // Otherwise, look for the common prefix and suffix, but back up so
    // that we can speak complete words, to be minimally confusing.
    prefixLen = commonPrefixLen;
    while (prefixLen < prev.value.length && prefixLen < evt.value.length &&
           prev.value[prefixLen] === evt.value[prefixLen]) {
      prefixLen++;
    }
    while (prefixLen > 0 &&
           !StringUtil.isWordBreakChar(prev.value[prefixLen - 1])) {
      prefixLen--;
    }

    // For suffix, commonSuffixLen is not used because suffix here won't overlap
    // with prefix, and also we need to consider |autocompleteSuffix|.
    suffixLen = 0;
    while (suffixLen < (prev.value.length - prefixLen) &&
           suffixLen < (evt.value.length - prefixLen) &&
           prev.value[prev.value.length - suffixLen - 1] ===
               evt.value[evt.value.length - suffixLen - 1]) {
      suffixLen++;
    }
    while (suffixLen > 0 &&
           !StringUtil.isWordBreakChar(
               prev.value[prev.value.length - suffixLen])) {
      suffixLen--;
    }

    this.describeTextChangedHelper(
        prev, evt, prefixLen, suffixLen, '', personality);
  }

  /**
   * The function is called by describeTextChanged and process if there's
   * some text changes likely made by IME.
   * @param {TextChangeEvent} prev The previous text change event.
   * @param {TextChangeEvent} evt The text change event.
   * @param {number} commonPrefixLen The number of characters in the common
   *     prefix of this.value and newValue.
   * @param {number} commonSuffixLen The number of characters in the common
   *     suffix of this.value and newValue.
   * @return {boolean} True if the event was processed.
   */
  describeTextChangedByIME(prev, evt, commonPrefixLen, commonSuffixLen) {
    // This supports typing Echo with IME.
    // - no selection range before and after.
    // - suffixes are common after both cursor end.
    // - prefixes are common at least max(0, "before length - 3").
    // Then, something changed in composition range. Announce the new
    // characters.
    const relaxedPrefixLen = Math.max(
        prev.start - ChromeVoxEditableTextBase.MAX_CHANGE_CHARS_BY_SINGLE_TYPE,
        0);
    let suffixLen = evt.value.length - evt.end;
    if (prev.start === prev.end && evt.start === evt.end &&
        prev.value.length - prev.end === suffixLen &&
        commonPrefixLen >= relaxedPrefixLen && commonPrefixLen < evt.start &&
        commonSuffixLen >= suffixLen) {
      if (LocalStorage.get('typingEcho') === TypingEchoState.CHARACTER ||
          LocalStorage.get('typingEcho') ===
              TypingEchoState.CHARACTER_AND_WORD) {
        this.speak(
            evt.value.substring(commonPrefixLen, evt.start),
            evt.triggeredByUser);
      }
      return true;
    }

    // The followings happens when a user starts to select candidates.
    // - no selection range before and after.
    // - prefixes are common before "new cursor point".
    // - suffixes are common after "old cursor point".
    // Then, this suggests that pressing a space or a tab to start composition.
    // Let's announce the first suggested content.
    // Note that after announcing this, announcements will be made by candidate
    // window's selection event instead of ChromeVox's editable.
    const prefixLen = evt.start;
    suffixLen = prev.value.length - prev.end;
    if (prev.start === prev.end && evt.start === evt.end &&
        evt.start < prev.start && evt.value.length > prefixLen + suffixLen &&
        commonPrefixLen >= prefixLen && commonSuffixLen >= suffixLen) {
      this.speak(
          evt.value.substring(prefixLen, evt.value.length - suffixLen),
          evt.triggeredByUser,
          new TtsSpeechProperties({'phoneticCharacters': true}));
      return true;
    }

    return false;
  }

  /**
   * The function is called by describeTextChanged after it's figured out
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
      if ((LocalStorage.get('typingEcho') === TypingEchoState.WORD ||
           LocalStorage.get('typingEcho') ===
               TypingEchoState.CHARACTER_AND_WORD) &&
          StringUtil.isWordBreakChar(inserted) && prefixLen > 0 &&
          !StringUtil.isWordBreakChar(evt.value.substr(prefixLen - 1, 1))) {
        // Speak previous word.
        let index = prefixLen;
        while (index > 0 && !StringUtil.isWordBreakChar(evt.value[index - 1])) {
          index--;
        }
        if (index < prefixLen) {
          utterance = evt.value.substr(index, prefixLen + 1 - index);
        } else {
          utterance = inserted;
          triggeredByUser = false;  // Implies QUEUE_MODE_QUEUE.
        }
      } else if (
          LocalStorage.get('typingEcho') === TypingEchoState.CHARACTER ||
          LocalStorage.get('typingEcho') ===
              TypingEchoState.CHARACTER_AND_WORD) {
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

/**
 * The maximum number of characters that can be changed by typing a character.
 * This is not 1, because some IME, especially Japanese, have a complex typing
 * system.
 * For example, typing 'u' after 'xts' will be converted into 'っ'
 * @const {number}
 */
ChromeVoxEditableTextBase.MAX_CHANGE_CHARS_BY_SINGLE_TYPE = 3;
