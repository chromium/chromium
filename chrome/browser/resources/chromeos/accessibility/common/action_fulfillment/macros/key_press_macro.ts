// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EventGenerator} from '../../event_generator.js';
import {KeyCode} from '../../key_code.js';

import type {RunMacroResult} from './macro.js';
import {Macro, MacroError} from './macro.js';
import type {MacroName} from './macro_names.js';

export interface KeyCombination {
  key: KeyCode;
  keyDisplay?: string;
  modifiers?: chrome.accessibilityPrivate.SyntheticKeyboardModifiers;
}

/**
 * Key codes accepted in a custom key combination. KeyCode.UNKNOWN (0, which
 * also aliases KeyCode.POWER_BUTTON) is excluded because Settings never
 * produces it from a real key press.
 */
const VALID_KEY_CODES: ReadonlySet<number> =
    new Set<number>(Object.values(KeyCode).filter(
        (v): v is number => typeof v === 'number' && v !== KeyCode.UNKNOWN));

/** Valid modifier key names on KeyCombination. */
const VALID_MODIFIER_KEYS: ReadonlySet<string> =
    new Set<keyof chrome.accessibilityPrivate.SyntheticKeyboardModifiers>(
        ['ctrl', 'alt', 'shift', 'search']);

/** Class that implements a macro to send a synthetic key event. */
export class KeyPressMacro extends Macro {
  private runCount_ = 0;
  private repeatTimerId_: number|undefined;
  private keyCombination_: KeyCombination;

  constructor(macroName: MacroName, keyCombination: KeyCombination) {
    super(macroName);
    this.keyCombination_ = keyCombination;
  }

  getKeyCombination(): KeyCombination {
    return this.keyCombination_;
  }

  private clearInterval_(): void {
    if (this.repeatTimerId_ !== undefined) {
      clearInterval(this.repeatTimerId_);
      this.repeatTimerId_ = undefined;
    }
  }

  /**
   * The key press macro should be run twice, once at keydown and once at keyup.
   */
  override triggersAtActionStartAndEnd(): boolean {
    return true;
  }

  override run(): RunMacroResult {
    const key = this.keyCombination_.key;
    const modifiers = this.keyCombination_.modifiers || {};
    // To avoid the modifiers getting stripped away, do not use rewriters if
    // there are modifiers.
    const useRewriters = this.keyCombination_.modifiers ? false : true;
    if (this.runCount_ === 0) {
      EventGenerator.sendKeyDown(key, modifiers, useRewriters);
      // Start sending repeat events on an interval.
      // If the gesture ends before the interval is reached, then a single key
      // press will be generated.
      this.repeatTimerId_ = setInterval(
          () => EventGenerator.sendKeyDown(
              key, modifiers, useRewriters, /*isRepeat=*/ true),
          KeyPressMacro.REPEAT_MS);
    } else if (this.runCount_ === 1) {
      this.clearInterval_();
      EventGenerator.sendKeyUp(key, modifiers, useRewriters);
    } else {
      console.error('Key press macro cannot be run more than twice.');
      return this.createRunMacroResult_(
          /*isSuccess=*/ false, MacroError.INVALID_USER_INTENT);
    }
    this.runCount_++;
    return this.createRunMacroResult_(/*isSuccess=*/ true);
  }
}

export namespace KeyPressMacro {
  /**
   * The rate at which repeat key down events should be sent while the key
   * press macro is being held.
   */
  export const REPEAT_MS = 700;

  /**
   * Parses and validates a serialized KeyCombination JSON string from prefs.
   * Returns null if the input is not a valid KeyCombination.
   */
  export function parseAndValidateKeyCombination(rawJson: unknown):
      KeyCombination|null {
    if (typeof rawJson !== 'string') {
      return null;
    }

    let parsed: Partial<KeyCombination>;
    try {
      parsed = JSON.parse(rawJson);
    } catch {
      return null;
    }

    if (typeof parsed !== 'object' || parsed === null ||
        Array.isArray(parsed)) {
      return null;
    }

    if (typeof parsed.key !== 'number' || !Number.isInteger(parsed.key) ||
        !VALID_KEY_CODES.has(parsed.key)) {
      return null;
    }

    if (parsed.keyDisplay !== undefined &&
        typeof parsed.keyDisplay !== 'string') {
      return null;
    }

    if (parsed.modifiers !== undefined) {
      if (typeof parsed.modifiers !== 'object' || parsed.modifiers === null ||
          Array.isArray(parsed.modifiers)) {
        return null;
      }
      for (const [key, value] of Object.entries(parsed.modifiers)) {
        if (!VALID_MODIFIER_KEYS.has(key) || typeof value !== 'boolean') {
          return null;
        }
      }
    }

    return parsed as KeyCombination;
  }
}
