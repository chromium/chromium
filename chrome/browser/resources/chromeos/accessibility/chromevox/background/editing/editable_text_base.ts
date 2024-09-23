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
import {LocalStorage} from '/common/local_storage.js';
import {StringUtil} from '/common/string_util.js';
import {TestImportManager} from '/common/testing/test_import_manager.js';

import {Personality, QueueMode, TtsCategory, TtsSpeechProperties} from '../../common/tts_types.js';
import {TtsInterface} from '../tts_interface.js';

import {TypingEchoState} from './typing_echo.js';

/**
 * A class containing the information needed to speak
 * a text change event to the user.
 */
export class TextChangeEvent {
  private value_ = '';
  start: number;
  end: number;
  triggeredByUser: boolean;

  /**
   * @param newValue The new string value of the editable text control.
   * @param newStart The new 0-based start cursor/selection index.
   * @param newEnd The new 0-based end cursor/selection index.
   */
  constructor(
      newValue: string, newStart: number, newEnd: number,
      triggeredByUser: boolean) {
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

  get value(): string {
    return this.value_;
  }
  set value(val: string) {
    this.value_ = val.replace(/\u00a0/g, ' ');
  }
}

/**
 * A class representing an abstracted editable text control.
 */
export class ChromeVoxEditableTextBase {
  static shouldSpeakInsertions = false;
  static maxShortPhraseLen = 60;

  /** Current value of the text field. */
  private value_ = '';
  /** 0-based selection start index. */
  protected start: number;
  /** 0-based selection end index. */
  protected end: number;
  /** True if this is a password field. */
  protected isPassword: boolean;
  /** Text-to-speech object implementing speak() and stop() methods. */
  protected tts: TtsInterface;
  /** Whether or not the text field is multiline. */
  protected multiline = false;
  /**
   * Whether or not the last update to the text and selection was described.
   *
   * Some consumers of this flag like |ChromeVoxEventWatcher| depend on and
   * react to when this flag is false by generating alternative feedback.
   */
  lastChangeDescribed = false;

  /**
   * @param value The string value of the editable text control.
   * @param start The 0-based start cursor/selection index.
   * @param end The 0-based end cursor/selection index.
   * @param isPassword Whether the text control if a password field.
   */
  constructor(
      value: string, start: number, end: number, isPassword: boolean,
      tts: TtsInterface) {
    this.value = value;
    this.start = start;
    this.end = end;
    this.isPassword = isPassword;
    this.tts = tts;
  }

  get value(): string {
    return this.value_;
  }

  set value(newValue: string) {
    this.value_ = newValue.replace('\u00a0', ' ');
  }

  getLineIndex(_charIndex: number): number {
    return 0;
  }

  getLineStart(_lineIndex: number): number {
    return 0;
  }

  getLineEnd(_lineIndex: number): number {
    return this.value.length;
  }

  /**
   * Get the full text of the current line.
   * @param index The 0-based line index.
   * @return The text of the line.
   */
  getLine(index: number): string {
    const lineStart = this.getLineStart(index);
    const lineEnd = this.getLineEnd(index);
    return this.value.substr(lineStart, lineEnd - lineStart);
  }

  /**
   * Speak text, but if it's a single character, describe the character.
   * @param str The string to speak.
   * @param triggeredByUser True if the speech was triggered by a user action.
   * @param personality Personality used to speak text.
   */
  speak(str: string, triggeredByUser?: boolean,
        personality?: TtsSpeechProperties): void {
    if (!str) {
      return;
    }
    let queueMode = QueueMode.QUEUE;
    if (triggeredByUser === true) {
      queueMode = QueueMode.CATEGORY_FLUSH;
    }
    const props = personality ?? new TtsSpeechProperties();
    props.category = TtsCategory.NAV;
    this.tts.speak(str, queueMode, props);
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
  describeTextChangedByIME(
      prev: TextChangeEvent, evt: TextChangeEvent, commonPrefixLen: number,
      commonSuffixLen: number): boolean {
    // This supports typing Echo with IME.
    // - no selection range before and after.
    // - suffixes are common after both cursor end.
    // - prefixes are common at least max(0, "before length - 3").
    // Then, something changed in composition range. Announce the new
    // characters.
    const relaxedPrefixLen =
        Math.max(prev.start - MAX_CHANGE_CHARS_BY_SINGLE_TYPE, 0);
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
   * @param {TtsSpeechProperties=} personality Personality to speak the
   *     text.
   */
  describeTextChangedHelper(
      prev: TextChangeEvent, evt: TextChangeEvent, prefixLen: number,
      suffixLen: number, autocompleteSuffix: string,
      personality?: TtsSpeechProperties): void {
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
        while (index > 0 &&
               !StringUtil.isWordBreakChar(evt.value[index - 1]!)) {
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
      personality = Personality.DELETED;
    }

    if (autocompleteSuffix && utterance) {
      utterance += ', ' + autocompleteSuffix;
    } else if (autocompleteSuffix) {
      utterance = autocompleteSuffix;
    }

    if (utterance) {
      this.speak(utterance, triggeredByUser, personality);
    }
  }
}

// Private to module.

const MAX_CHANGE_CHARS_BY_SINGLE_TYPE = 3;

TestImportManager.exportForTesting(ChromeVoxEditableTextBase, TextChangeEvent);
