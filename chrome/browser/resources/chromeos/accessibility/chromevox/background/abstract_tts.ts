// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Msgs} from '../common/msgs.js';
import {SettingsManager} from '../common/settings_manager.js';
import * as ttsTypes from '../common/tts_types.js';

import {TtsCapturingEventListener, TtsInterface} from './tts_interface.js';

interface PropertyValues {
  pitch: number;
  rate: number;
  volume: number;

  [key: string]: number | undefined;
}

interface Properties {
  [key: string]: number | undefined;
}

/**
 * Base class for Text-to-Speech engines that actually transform
 * text to speech (as opposed to logging or other behaviors).
 */
export class AbstractTts implements TtsInterface {
  /**
   * Default value for TTS properties.
   * Note that these as well as the subsequent properties might be different
   * on different host platforms (like Chrome, Android, etc.).
   */
  protected propertyDefault: PropertyValues;
  /** Min value for TTS properties. */
  protected propertyMin: PropertyValues;
  /** Max value for TTS properties. */
  protected propertyMax: PropertyValues;
  /** Step value for TTS properties. */
  protected propertyStep: PropertyValues;
  /** Default TTS properties for this TTS engine. */
  protected ttsProperties: Properties = {};

  /** Substitution dictionary regexp. */
  private static substitutionDictionaryRegexp_: RegExp;
  /** Repetition filter regexp. */
  private static repetitionRegexp_: RegExp =
      /([-\/\\|!@#$%^&*\(\)=_+\[\]\{\}.?;'":<>\u2022\u25e6\u25a0])\1{2,}/g;
  /** Regexp filter for negative dollar and pound amounts. */
  private static negativeCurrencyAmountRegexp_: RegExp =
      /-[£\$](\d{1,3})(\d+|(,\d{3})*)(\.\d{1,})?/g;

  constructor() {
    const pitchDefault = 1;
    const pitchMin = 0.2;
    const pitchMax = 2.0;
    const pitchStep = 0.1;

    const rateDefault = 1;
    const rateMin = 0.2;
    const rateMax = 5.0;
    const rateStep = 0.1;

    const volumeDefault = 1;
    const volumeMin = 0.2;
    const volumeMax = 1.0;
    const volumeStep = 0.1;

    this.propertyDefault = {
      pitch: pitchDefault,
      rate: rateDefault,
      volume: volumeDefault,
    };

    this.propertyMin = {
      pitch: pitchMin,
      rate: rateMin,
      volume: volumeMin,
    };

    this.propertyMax = {
      pitch: pitchMax,
      rate: rateMax,
      volume: volumeMax,
    };

    this.propertyStep = {rate: rateStep, pitch: pitchStep, volume: volumeStep};

    if (AbstractTts.substitutionDictionaryRegexp_ === undefined) {
      // Create an expression that matches all words in the substitution
      // dictionary.
      const symbols: string[] = [];
      for (const symbol in ttsTypes.SubstitutionDictionary) {
        symbols.push(symbol);
      }
      const expr = '(' + symbols.join('|') + ')';
      AbstractTts.substitutionDictionaryRegexp_ = new RegExp(expr, 'ig');
    }
  }

  /** TtsInterface implementation. */
  speak(
      _textString: string, _queueMode: ttsTypes.QueueMode,
      _properties?: ttsTypes.TtsSpeechProperties): AbstractTts {
    return this;
  }

  /** TtsInterface implementation. */
  isSpeaking(): boolean {
    return false;
  }

  /** TtsInterface implementation. */
  stop(): void {}

  /** TtsInterface implementation. */
  addCapturingEventListener(_listener: TtsCapturingEventListener): void {}

  /** TtsInterface implementation. */
  removeCapturingEventListener(_listener: TtsCapturingEventListener): void {}

  /** TtsInterface implementation. */
  increaseOrDecreaseProperty(propertyName: string, increase: boolean): void {
    // TODO(b/314203187): Not null asserted, check that this is correct.
    const step = this.propertyStep[propertyName]!;
    let current = this.ttsProperties[propertyName]!;
    current = increase ? current + step : current - step;
    this.setProperty(propertyName, current);
  }

  /** TtsInterface implementation. */
  setProperty(propertyName: string, value: number): void {
    // TODO(b/314203187): Not null asserted, check that this is correct.
    const min = this.propertyMin[propertyName]!;
    const max = this.propertyMax[propertyName]!;
    this.ttsProperties[propertyName] = Math.max(Math.min(value, max), min);
  }

  /**
   * Converts an engine property value to a percentage from 0.00 to 1.00.
   * @param property The property to convert.
   * @return The percentage of the property.
   */
  propertyToPercentage(property: string): number|null {
    // TODO(b/314203187): Not null asserted, check that this is correct.
    return (this.ttsProperties[property]! - this.propertyMin[property]!) /
        Math.abs(this.propertyMax[property]! - this.propertyMin[property]!);
  }

  /**
   * Merges the given properties with the default ones. Always returns a
   * new object, so that you can safely modify the result of mergeProperties
   * without worrying that you're modifying an object used elsewhere.
   * @param properties The properties to merge with the current ones.
   * @return The merged properties.
   */
  protected mergeProperties(properties: Properties): Properties {
    const mergedProperties: Properties = {};
    let p;
    if (this.ttsProperties) {
      for (p in this.ttsProperties) {
        mergedProperties[p] = this.ttsProperties[p];
      }
    }
    if (properties) {
      const tts = ttsTypes.TtsSettings;
      if (typeof (properties[tts.VOLUME]) === 'number') {
        mergedProperties[tts.VOLUME] = properties[tts.VOLUME];
      }
      if (typeof (properties[tts.PITCH]) === 'number') {
        mergedProperties[tts.PITCH] = properties[tts.PITCH];
      }
      if (typeof (properties[tts.RATE]) === 'number') {
        mergedProperties[tts.RATE] = properties[tts.RATE];
      }
      if (typeof (properties[tts.LANG]) === 'string') {
        mergedProperties[tts.LANG] = properties[tts.LANG];
      }

      const context = this;
      const mergeRelativeProperty = function(abs: string, rel: string): void {
        if (typeof (properties[rel]) === 'number' &&
            typeof (mergedProperties[abs]) === 'number') {
          mergedProperties[abs] += properties[rel];
          // TODO(b/314203187): Not null asserted, check that this is correct.
          const min = context.propertyMin[abs]!;
          const max = context.propertyMax[abs]!;
          if (mergedProperties[abs] > max) {
            mergedProperties[abs] = max;
          } else if (mergedProperties[abs] < min) {
            mergedProperties[abs] = min;
          }
        }
      };

      mergeRelativeProperty(tts.VOLUME, tts.RELATIVE_VOLUME);
      mergeRelativeProperty(tts.PITCH, tts.RELATIVE_PITCH);
      mergeRelativeProperty(tts.RATE, tts.RELATIVE_RATE);
    }

    for (p in properties) {
      if (!mergedProperties.hasOwnProperty(p)) {
        mergedProperties[p] = properties[p];
      }
    }

    return mergedProperties;
  }

  /**
   * Method to preprocess text to be spoken properly by a speech
   * engine.
   *
   * 1. Replace any single character with a description of that character.
   *
   * 2. Convert all-caps words to lowercase if they don't look like an
   *    acronym / abbreviation.
   *
   * @param text A text string to be spoken.
   * @param properties Out parameter populated with how to speak the string.
   * @return The text formatted in a way that will sound better by most speech
   *     engines.
   */
  protected preprocess(text: string, properties: Properties = {}): string {
    if (text.length === 1 && text.toLowerCase() !== text) {
      // Describe capital letters according to user's setting.
      if (SettingsManager.getString('capitalStrategy') === 'increasePitch') {
        // Closure doesn't allow the use of for..in or [] with structs, so
        // convert to a pure JSON object.
        const CAPITAL = ttsTypes.Personality.CAPITAL.toJSON() as PropertyValues;
        for (const prop in CAPITAL) {
          if (properties[prop] === undefined) {
            properties[prop] = CAPITAL[prop];
          }
        }
      } else if (
          SettingsManager.getString('capitalStrategy') === 'announceCapitals') {
        text = Msgs.getMsg('announce_capital_letter', [text]);
      }
    }

    if (!SettingsManager.getBoolean('usePitchChanges')) {
      delete properties['relativePitch'];
    }

    // Since dollar and sterling pound signs will be replaced with text, move
    // them to after the number if they stay between a negative sign and a
    // number.
    text = text.replace(AbstractTts.negativeCurrencyAmountRegexp_, match => {
      const minus = match[0];
      const number = match.substring(2);
      const currency = match[1];

      return minus + number + currency;
    });

    // Substitute all symbols in the substitution dictionary. This is pretty
    // efficient because we use a single regexp that matches all symbols
    // simultaneously.
    text = text.replace(
        AbstractTts.substitutionDictionaryRegexp_, function(symbol) {
          return ' ' + ttsTypes.SubstitutionDictionary[symbol] + ' ';
        });

    // Handle single characters that we want to make sure we pronounce.
    if (text.length === 1) {
      return ttsTypes.CharacterDictionary[text] ?
          Msgs.getMsgWithCount(ttsTypes.CharacterDictionary[text], 1) :
          text.toUpperCase();
    }

    // Expand all repeated characters.
    text = text.replace(
        AbstractTts.repetitionRegexp_, AbstractTts.repetitionReplace_);

    return text;
  }

  /**
   * Constructs a description of a repeated character. Use as a param to
   * string.replace.
   * @param match The matching string.
   * @return The description.
   */
  private static repetitionReplace_(match: string): string {
    const count = match.length;
    return ' ' +
        Msgs.getMsgWithCount(ttsTypes.CharacterDictionary[match[0]], count) +
        ' ';
  }

  /** TtsInterface implementation. */
  getDefaultProperty(property: string): number {
    // TODO(b/314203187): Not null asserted, check that this is correct.
    return this.propertyDefault[property]!;
  }

  /** TtsInterface implementation. */
  toggleSpeechOnOrOff(): boolean {
    return true;
  }
}