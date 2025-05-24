// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Default implementation for TTS in the background context.
 */
import {constants} from '/common/constants.js';
import {LocalStorage} from '/common/local_storage.js';
import {TestImportManager} from '/common/testing/test_import_manager.js';

import {Msgs} from '../common/msgs.js';
import {PanelCommand, PanelCommandType} from '../common/panel_command.js';
import {SettingsManager} from '../common/settings_manager.js';
import {CharacterDictionary, Personality, PunctuationEcho, PunctuationEchoes, QueueMode, TtsAudioProperty, TtsSettings, TtsSpeechProperties} from '../common/tts_types.js';

import {AbstractTts} from './abstract_tts.js';
import {ChromeVox} from './chromevox.js';
import {PhoneticData} from './phonetic_data.js';
import {TtsCapturingEventListener, TtsInterface} from './tts_interface.js';

type TtsVoice = chrome.tts.TtsVoice;

class Utterance {
  static nextUtteranceId_ = 1;
  textString: string;
  properties: TtsSpeechProperties;
  queueMode: QueueMode;
  id: number;

  /**
   * @param textString The string of text to be spoken.
   * @param properties Speech properties to use for this utterance.
   */
  constructor(
      textString: string, properties: TtsSpeechProperties,
      queueMode: QueueMode) {
    this.textString = textString;
    this.properties = properties;
    this.queueMode = queueMode;
    this.id = Utterance.nextUtteranceId_++;
  }
};

/**
 * This class is the default implementation for TTS in the background context.
 */
export class PrimaryTts extends AbstractTts {
  // The amount of time, in milliseconds, to wait before speaking a hint.
  static hint_delay_ms_ = 1000;
  /**
   * The list of properties allowed to be passed to the chrome.tts.speak API.
   * Anything outside this list will be stripped.
   */
  private static ALLOWED_PROPERTIES_ = [
    'desiredEventTypes',
    'enqueue',
    'extensionId',
    'gender',
    'lang',
    'onEvent',
    'pitch',
    'rate',
    'requiredEventTypes',
    'voiceName',
    'volume',
  ];
  private static SKIP_WHITESPACE_ = /^[\s\u00a0]*$/;

  // The current voice name.
  currentVoice: string|undefined;
  lastEventType = chrome.tts.EventType.END;

  private currentPunctuationEcho_: number;
  /**
   * A list of punctuation characters that should always be spliced into
   * output even with literal word substitutions. This is important for tts
   * prosity.
   */
  private retainPunctuation_ = [';', '?', '!', '\''];
  // The id of a callback returned by setTimeout.
  private timeoutId_: number|undefined;
  // Capturing tts event listeners.
  private capturingTtsEventListeners_: TtsCapturingEventListener[] = [];
  // The current utterance.
  private currentUtterance_: Utterance|null = null;
  // The utterance queue.
  private utteranceQueue_: Utterance[] = [];
  // Queue of utterances interrupted by interjected utterances.
  private utteranceQueueInterruptedByInterjection_: Utterance[] = [];


  constructor() {
    super();

    this.currentPunctuationEcho_ =
        SettingsManager.getNumber(TtsSettings.PUNCTUATION_ECHO);


    if (window.speechSynthesis) {
      window.speechSynthesis.onvoiceschanged = () =>
          this.updateVoice(SettingsManager.getString('voiceName'));
    } else {
      // SpeechSynthesis API is not available on chromecast. Call
      // updateVoice to set the one and only voice as the current
      // voice.
      this.updateVoice('');
    }

    SettingsManager.addListenerForKey(
        'voiceName', voiceName => this.updateVoice(voiceName));

    // Migration: local LocalStorage tts properties -> Chrome pref settings.
    if (LocalStorage.get('rate')) {
      chrome.settingsPrivate.setPref(
          'settings.tts.speech_rate', LocalStorage.get('rate'));
      LocalStorage.remove('rate');
    }
    if (LocalStorage.get('pitch')) {
      chrome.settingsPrivate.setPref(
          'settings.tts.speech_pitch', LocalStorage.get('pitch'));
      LocalStorage.remove('pitch');
    }
    if (LocalStorage.get('volume')) {
      chrome.settingsPrivate.setPref(
          'settings.tts.speech_volume', LocalStorage.get('volume'));
      LocalStorage.remove('volume');
    }

    // At startup.
    chrome.settingsPrivate.getAllPrefs(
        prefs => this.updateFromPrefs_(false, prefs));

    // At runtime.
    chrome.settingsPrivate.onPrefsChanged.addListener(
        prefs => this.updateFromPrefs_(true, prefs));
  }

  /**
   * @param textString The string of text to be spoken.
   * @param queueMode The queue mode to use for speaking.
   * @param properties Speech properties to use
   *     for this utterance.
   * @return A tts object useful for chaining speak calls.
   */
  override speak(
      textString: string, queueMode: QueueMode,
      properties?: TtsSpeechProperties): TtsInterface {
    super.speak(textString, queueMode, properties);

    // |textString| gets manipulated throughout this function. Save the
    // original value for functions that may need it.
    const originalTextString = textString;

    if (this.ttsProperties[TtsSettings.VOLUME] === 0) {
      return this;
    }

    if (!properties) {
      properties = new TtsSpeechProperties();
    }

    if (textString.length > constants.OBJECT_MAX_CHARCOUNT) {
      // The text is too long. Try to split the text into multiple chunks
      // based on line breaks.
      this.speakSplittingText_(textString, queueMode, properties);
      return this;
    }

    textString = this.preprocess(textString, properties);

    // This pref on SettingsManager gets set by the options page.
    if (SettingsManager.get('numberReadingStyle') === 'asDigits') {
      textString = this.getNumberAsDigits_(textString);
    }

    // TODO(dtseng): some TTS engines don't handle strings that don't produce
    // any speech very well. Handle empty and whitespace only strings
    // (including non-breaking space) here to mitigate the issue somewhat.
    if (PrimaryTts.SKIP_WHITESPACE_.test(textString)) {
      // Explicitly call start and end callbacks before skipping this text.
      if (properties.startCallback) {
        try {
          properties.startCallback();
        } catch (e) {
        }
      }
      if (properties.endCallback) {
        try {
          properties.endCallback();
        } catch (e) {
        }
      }
      if (queueMode === QueueMode.FLUSH) {
        this.stop();
      }
      return this;
    }

    const mergedProperties = this.mergeProperties(properties);

    if (this.currentVoice && this.currentVoice !== constants.SYSTEM_VOICE) {
      mergedProperties['voiceName'] = this.currentVoice;
    }

    const utterance = new Utterance(textString, mergedProperties, queueMode);
    this.speakUsingQueue_(utterance);
    // Attempt to queue phonetic speech with property['delay']. This ensures
    // that phonetic hints are delayed when we process them.
    this.pronouncePhonetically_(originalTextString, properties);
    return this;
  }

  getUtteranceQueueForTest(): Utterance[] {
    return this.utteranceQueue_;
  }

  /**
   * Split the given textString into smaller chunks and call this.speak() for
   * each chunks.
   * @param textString The string of text to be spoken.
   * @param queueMode The queue mode to use for speaking.
   * @param properties Speech properties to use for this utterance.
   */
  private speakSplittingText_(
      textString: string, queueMode: QueueMode,
      properties?: TtsSpeechProperties): void {
    const chunks = PrimaryTts.splitUntilSmall(textString, '\n\r ');
    for (const chunk of chunks) {
      this.speak(chunk, queueMode, properties);
      queueMode = QueueMode.QUEUE;
    }
  }

  /**
   * Splits |text| until each substring's length is smaller than or equal to
   * constants.OBJECT_MAX_CHARCOUNT.
   */
  static splitUntilSmall(text: string, delimiters: string): string[] {
    if (text.length === 0) {
      return [];
    }

    if (text.length <= constants.OBJECT_MAX_CHARCOUNT) {
      return [text];
    }

    const midIndex = text.length / 2;
    if (!delimiters) {
      return PrimaryTts.splitUntilSmall(text.substring(0, midIndex), delimiters)
          .concat(PrimaryTts.splitUntilSmall(
              text.substring(midIndex, text.length), delimiters));
    }

    const delimiter = delimiters[0];
    let splitIndex = text.lastIndexOf(delimiter, midIndex);
    if (splitIndex === -1) {
      splitIndex = text.indexOf(delimiter, midIndex);
    }

    if (splitIndex === -1) {
      delimiters = delimiters.slice(1);
      return PrimaryTts.splitUntilSmall(text, delimiters);
    }

    return PrimaryTts.splitUntilSmall(text.substring(0, splitIndex), delimiters)
        .concat(PrimaryTts.splitUntilSmall(
            text.substring(splitIndex + 1, text.length), delimiters));
  }

  /**
   * Use the speech queue to handle the given speech request.
   * @param utterance The utterance to speak.
   */
  private speakUsingQueue_(utterance: Utterance): void {
    const queueMode = utterance.queueMode;

    // First, take care of removing the current utterance and flushing
    // anything from the queue we need to. If we remove the current utterance,
    // make a note that we're going to stop speech.
    if (queueMode === QueueMode.FLUSH ||
        queueMode === QueueMode.CATEGORY_FLUSH ||
        queueMode === QueueMode.INTERJECT) {
      (new PanelCommand(PanelCommandType.CLEAR_SPEECH)).send();

      if (this.shouldCancel_(this.currentUtterance_, utterance)) {
        // Clear timeout in case currentUtterance_ is a delayed utterance.
        this.clearTimeout_();
        this.cancelUtterance_(this.currentUtterance_);
        this.currentUtterance_ = null;
      }
      let i = 0;
      while (i < this.utteranceQueue_.length) {
        if (this.shouldCancel_(this.utteranceQueue_[i], utterance)) {
          this.cancelUtterance_(this.utteranceQueue_[i]);
          this.utteranceQueue_.splice(i, 1);
        } else {
          i++;
        }
      }
    }

    // Now, some special handling for interjections.
    if (queueMode === QueueMode.INTERJECT) {
      // Move all utterances to a secondary queue to be restored later.
      this.utteranceQueueInterruptedByInterjection_ = this.utteranceQueue_;

      // The interjection is the only utterance.
      this.utteranceQueue_ = [utterance];

      // Ensure to clear the current utterance and prepend it for it to repeat
      // later.
      if (this.currentUtterance_) {
        this.utteranceQueueInterruptedByInterjection_.unshift(
            this.currentUtterance_);
        this.currentUtterance_ = null;
      }

      // Restore the interrupted utterances after allowing all other
      // utterances in this callstack to process.
      setTimeout(() => {
        // Utterances on the current queue are now also interjections.
        for (let i = 0; i < this.utteranceQueue_.length; i++) {
          this.utteranceQueue_[i].queueMode = QueueMode.INTERJECT;
        }
        this.utteranceQueue_ = this.utteranceQueue_.concat(
            this.utteranceQueueInterruptedByInterjection_);
      }, 0);
    } else {
      // Next, add the new utterance to the queue.
      this.utteranceQueue_.push(utterance);
    }

    // Now start speaking the next item in the queue.
    this.startSpeakingNextItemInQueue_();
  }

  /**
   * If nothing is speaking, pop the first item off the speech queue and
   * start speaking it. This is called when a speech request is made and
   * when the current utterance finishes speaking.
   */
  private startSpeakingNextItemInQueue_(): void {
    if (this.currentUtterance_) {
      return;
    }

    if (this.utteranceQueue_.length === 0) {
      return;
    }

    // There is no voice to speak with (e.g. the tts system has not fully
    // initialized).
    if (!this.currentVoice) {
      return;
    }

    // Clear timeout for delayed utterances (hints and phonetic speech).
    this.clearTimeout_();

    // Check top of utteranceQueue for delayed utterance.
    if (this.utteranceQueue_[0].properties['delay']) {
      // Remove 'delay' property and set a timeout to process this utterance
      // after the delay has passed.
      delete this.utteranceQueue_[0].properties['delay'];
      this.timeoutId_ = setTimeout(
          () => this.startSpeakingNextItemInQueue_(),
          PrimaryTts.hint_delay_ms_);

      return;
    }

    this.currentUtterance_ = this.utteranceQueue_.shift()!;
    const utterance = this.currentUtterance_;
    const utteranceId = utterance.id;

    utterance.properties['onEvent'] = event => {
      this.onTtsEvent_(event, utteranceId);
    };

    const validatedProperties: chrome.tts.TtsOptions = {};
    for (let i = 0; i < PrimaryTts.ALLOWED_PROPERTIES_.length; i++) {
      const p = PrimaryTts.ALLOWED_PROPERTIES_[i];
      if (utterance.properties[p]) {
        (validatedProperties as any)[p] = utterance.properties[p];
      }
    }

    // Update the caption panel.
    if (utterance.properties && utterance.properties['pitch'] &&
        utterance.properties['pitch'] < this.ttsProperties['pitch']!) {
      (new PanelCommand(
           PanelCommandType.ADD_ANNOTATION_SPEECH, utterance.textString))
          .send();
    } else {
      (new PanelCommand(
           PanelCommandType.ADD_NORMAL_SPEECH, utterance.textString))
          .send();
    }

    chrome.tts.speak(utterance.textString, validatedProperties);
  }

  /**
   * Called when we get a speech event from Chrome. We ignore any event
   * that doesn't pertain to the current utterance, but when speech starts
   * or ends we optionally call callback functions, and start speaking the
   * next utterance if there's another one enqueued.
   * @param event The TTS event from chrome.
   * @param utteranceId The id of the associated utterance.
   */
  private onTtsEvent_(event: chrome.tts.TtsEvent, utteranceId: number): void {
    this.lastEventType = event['type'];

    // Ignore events sent on utterances other than the current one.
    if (!this.currentUtterance_ || utteranceId !== this.currentUtterance_.id) {
      return;
    }

    const utterance = this.currentUtterance_;

    switch (event['type']) {
      case chrome.tts.EventType.START:
        this.capturingTtsEventListeners_.forEach(
            listener => listener.onTtsStart());
        if (utterance.properties['startCallback']) {
          try {
            utterance.properties['startCallback']();
          } catch (e) {
          }
        }
        break;
      case chrome.tts.EventType.END:
        // End callbacks could cause additional speech to queue up.
        this.currentUtterance_ = null;
        this.capturingTtsEventListeners_.forEach(
            listener => listener.onTtsEnd());
        if (utterance.properties['endCallback']) {
          try {
            utterance.properties['endCallback']();
          } catch (e) {
          }
        }
        this.startSpeakingNextItemInQueue_();
        break;
      case chrome.tts.EventType.INTERRUPTED:
        this.cancelUtterance_(utterance);
        this.currentUtterance_ = null;
        for (let i = 0; i < this.utteranceQueue_.length; i++) {
          this.cancelUtterance_(this.utteranceQueue_[i]);
        }
        this.utteranceQueue_.length = 0;
        this.capturingTtsEventListeners_.forEach(
            listener => listener.onTtsInterrupted());
        break;
      case chrome.tts.EventType.ERROR:
        this.onError_(event['errorMessage']);
        this.currentUtterance_ = null;
        this.startSpeakingNextItemInQueue_();
        break;
    }
  }

  /**
   * Determines if |utteranceToCancel| should be canceled (interrupted if
   * currently speaking, or removed from the queue if not), given the new
   * utterance we want to speak and the queue mode. If the queue mode is
   * QUEUE or FLUSH, the logic is straightforward. If the queue mode is
   * CATEGORY_FLUSH, we only flush utterances with the same category.
   *
   * @param utteranceToCancel The utterance in question.
   * @param newUtterance The new utterance we're enqueueing.
   * @return True if this utterance should be canceled.
   */
  private shouldCancel_(
      utteranceToCancel: Utterance|null, newUtterance: Utterance): boolean {
    if (!utteranceToCancel) {
      return false;
    }
    if (utteranceToCancel.properties['doNotInterrupt']) {
      return false;
    }
    switch (newUtterance.queueMode) {
      case QueueMode.QUEUE:
        return false;
      case QueueMode.INTERJECT:
        return utteranceToCancel.queueMode === QueueMode.INTERJECT;
      case QueueMode.FLUSH:
        return true;
      case QueueMode.CATEGORY_FLUSH:
        return (
            utteranceToCancel.properties['category'] ===
            newUtterance.properties['category']);
    }
  }

  /**
   * Do any cleanup necessary to cancel an utterance, like callings its
   * callback function if any.
   * @param utterance The utterance to cancel.
   */
  private cancelUtterance_(utterance: Utterance|null): void {
    if (utterance && utterance.properties['endCallback']) {
      try {
        utterance.properties['endCallback'](true);
      } catch (e) {
      }
    }
  }

  override increaseOrDecreaseProperty(
      propertyName: TtsAudioProperty, increase: boolean): void {
    super.increaseOrDecreaseProperty(propertyName, increase);
    const value = this.ttsProperties[propertyName]!;
    this.setProperty(propertyName, value);
  }

  override setProperty(propertyName: TtsAudioProperty, value: number): void {
    super.setProperty(propertyName, value);
    let pref: string;
    switch (propertyName) {
      case TtsAudioProperty.RATE:
        pref = 'settings.tts.speech_rate';
        break;
      case TtsAudioProperty.PITCH:
        pref = 'settings.tts.speech_pitch';
        break;
      case TtsAudioProperty.VOLUME:
        pref = 'settings.tts.speech_volume';
        break;
      default:
        return;
    }
    chrome.settingsPrivate.setPref(pref, this.ttsProperties[propertyName]);
  }

  override isSpeaking(): boolean {
    super.isSpeaking();
    return Boolean(this.currentUtterance_);
  }

  override stop(): void {
    super.stop();

    this.cancelUtterance_(this.currentUtterance_);
    this.currentUtterance_ = null;

    for (let i = 0; i < this.utteranceQueue_.length; i++) {
      this.cancelUtterance_(this.utteranceQueue_[i]);
    }

    for (let i = 0; i < this.utteranceQueueInterruptedByInterjection_.length;
         i++) {
      this.cancelUtterance_(this.utteranceQueueInterruptedByInterjection_[i]);
    }

    this.utteranceQueue_.length = 0;
    this.utteranceQueueInterruptedByInterjection_.length = 0;

    (new PanelCommand(PanelCommandType.CLEAR_SPEECH)).send();
    chrome.tts.stop();

    this.capturingTtsEventListeners_.forEach(
        listener => listener.onTtsInterrupted());
  }

  override addCapturingEventListener(listener: TtsCapturingEventListener):
      void {
    this.capturingTtsEventListeners_.push(listener);
  }

  override removeCapturingEventListener(listener: TtsCapturingEventListener):
      void {
    this.capturingTtsEventListeners_ =
        this.capturingTtsEventListeners_.filter(item => {
          return item !== listener;
        });
  }

  /**
   * An error handler passed as a callback to chrome.tts.speak.
   * @param errorMessage Describes the error (set by onEvent).
   */
  private onError_(_errorMessage: string|undefined): void {
    this.updateVoice(this.currentVoice);
  }

  override preprocess(
      text: string,
      properties: TtsSpeechProperties = new TtsSpeechProperties()): string {
    // Perform generic processing.
    text = super.preprocess(text, properties);

    // Perform any remaining processing such as punctuation expansion.
    let punctEcho: PunctuationEcho;
    if (properties[TtsSettings.PUNCTUATION_ECHO]) {
      for (let i = 0; punctEcho = PunctuationEchoes[i]; i++) {
        if (properties[TtsSettings.PUNCTUATION_ECHO] === punctEcho.name) {
          break;
        }
      }
    } else {
      punctEcho = PunctuationEchoes[this.currentPunctuationEcho_];
    }
    text = text.replace(
        punctEcho.regexp, this.createPunctuationReplace_(punctEcho.clear));

    // Remove all whitespace from the beginning and end, and collapse all
    // inner strings of whitespace to a single space.
    text = text.replace(/\s+/g, ' ').replace(/^\s+|\s+$/g, '');

    // Look for the pattern [number + lowercase letter], such as in "5g
    // network". We want to capitalize the letter to prevent it from being
    // substituted with a unit in the TTS engine; in the above case, the
    // string would get spoken as "5 grams network", which we want to avoid.
    // We do not match against the letter "a" in the regular expression
    // because it is a word and we do not want to capitalize it just because
    // it comes after a number.
    text = text.replace(
        /(\d)(\s*)([b-z])\b/g,
        (_unused, num, whitespace, letter) =>
            num + whitespace + letter.toUpperCase());

    return text;
  }

  override toggleSpeechOnOrOff(): boolean {
    const previousValue = this.ttsProperties[TtsSettings.VOLUME];
    const toggle = () => {
      if (previousValue === 0) {
        this.ttsProperties[TtsSettings.VOLUME] = 1;
      } else {
        this.ttsProperties[TtsSettings.VOLUME] = 0;
        this.stop();
      }
    };

    if (previousValue === 0) {
      toggle();
    } else {
      // Let the caller make any last minute announcements in the current call
      // stack.
      setTimeout(toggle, 0);
    }

    return previousValue === 0;
  }

  /**
   * Method that updates the punctuation echo level, and also persists setting
   * to settings prefs.
   * @param punctuationEcho The index of the desired punctuation echo
   * level in PunctuationEchoes.
   */
  updatePunctuationEcho(punctuationEcho: number): void {
    this.currentPunctuationEcho_ = punctuationEcho;
    SettingsManager.set(TtsSettings.PUNCTUATION_ECHO, punctuationEcho);
  }

  /**
   * Method that cycles among the available punctuation echo levels.
   * @return The resulting punctuation level message id.
   */
  cyclePunctuationEcho(): string {
    this.updatePunctuationEcho(
        (this.currentPunctuationEcho_ + 1) % PunctuationEchoes.length);
    return PunctuationEchoes[this.currentPunctuationEcho_].msg;
  }

  /**
   * Converts a number into space-separated digits.
   * For numbers containing 4 or fewer digits, we return the original number.
   * This ensures that numbers like 123,456 or 2011 are not "digitized" while
   * 123456 is.
   * @param text The text to process.
   * @return A string with all numbers converted.
   * @private
   */
  getNumberAsDigits_(text: string): string {
    return text.replace(/[0-9０-９]+/g, function(num) {
      return num.split('').join(' ');
    });
  }

  /**
   * Constructs a function for string.replace that handles description of a
   *  punctuation character.
   * @param clear Whether we want to use whitespace in place of
   *     match.
   * @return The replacement function.
   */
  private createPunctuationReplace_(clear: boolean): (match: string) => string {
    return match => {
      const retain =
          this.retainPunctuation_.indexOf(match) !== -1 ? match : ' ';
      return clear ? retain :
                     ' ' + Msgs.getMsgWithCount(CharacterDictionary[match], 1) +
              retain + ' ';
    };
  }

  /**
   * Queues phonetic disambiguation for characters if disambiguation is found.
   * @param text The text for which we want to get phonetic data.
   * @param properties Speech properties to use for this utterance.
   * @private
   */
  private pronouncePhonetically_(text: string, properties: TtsSpeechProperties):
      void {
    // Math should never be pronounced phonetically.
    if (properties.math) {
      return;
    }

    // Only pronounce phonetic hints when explicitly requested.
    if (!properties.phoneticCharacters) {
      return;
    }

    // Remove this property so we don't trap ourselves in a loop.
    delete properties.phoneticCharacters;

    // If undefined language, use the UI language of the browser as a best
    // guess.
    if (!properties.lang) {
      properties.lang = chrome.i18n.getUILanguage();
    }

    const phoneticText = text.length === 1 ?
        PhoneticData.forCharacter(text, properties.lang!) :
        PhoneticData.forText(text, properties.lang!);
    if (phoneticText) {
      properties.delay = true;
      this.speak(phoneticText, QueueMode.QUEUE, properties);
    }
  }

  /**
   * Clears the last timeout set via setTimeout.
   */
  private clearTimeout_(): void {
    if (this.timeoutId_ !== undefined) {
      clearTimeout(this.timeoutId_);
      this.timeoutId_ = undefined;
    }
  }

  /**
   * Update the current voice used to speak based upon values in storage. If
   * that does not succeed, fallback to use system locale when picking a
   * voice.
   * @param voiceName Voice name to set.
   * @param opt_callback Called when the voice is determined.
   */
  updateVoice(
      voiceName: string|undefined,
      opt_callback?: (voices: string) => void): void {
    chrome.tts.getVoices((voices: TtsVoice[]) => {
      const systemVoice = {voiceName: constants.SYSTEM_VOICE};
      voices.unshift(systemVoice);
      const newVoice = voices.find(v => {
        return v.voiceName === voiceName;
      }) ||
          systemVoice;
      if (newVoice && newVoice.voiceName) {
        this.currentVoice = newVoice.voiceName;
        this.startSpeakingNextItemInQueue_();
      }
      if (opt_callback) {
        opt_callback(this.currentVoice!);
      }
    });
  }

  private updateFromPrefs_(
      announce: boolean, prefs: chrome.settingsPrivate.PrefObject[]): void {
    prefs.forEach(pref => {
      let msg: string;
      let propertyName: TtsAudioProperty;
      switch (pref.key) {
        case 'settings.tts.speech_rate':
          propertyName = TtsAudioProperty.RATE;
          msg = 'announce_rate';
          this.setHintDelayMS(/** @type {number} */ (pref.value));
          break;
        case 'settings.tts.speech_pitch':
          propertyName = TtsAudioProperty.PITCH;
          msg = 'announce_pitch';
          break;
        case 'settings.tts.speech_volume':
          propertyName = TtsAudioProperty.VOLUME;
          msg = 'announce_volume';
          break;
        default:
          return;
      }

      this.ttsProperties[propertyName] = pref.value;

      if (!announce) {
        return;
      }

      const valueAsPercent =
          Math.round((this.propertyToPercentage(propertyName) || 0) * 100);
      const announcement = Msgs.getMsg(msg, [valueAsPercent.toString()]);
      ChromeVox.tts.speak(
          announcement, QueueMode.FLUSH, Personality.ANNOTATION);
    });
  }

  /**
   * Sets |hint_delay_ms_| given the speech rate.
   * We want an inverse relationship between the speech rate and the hint
   * delay; the faster the speech rate, the shorter the delay should be.
   * Default speech rate (value of 1) should map to a delay of 1000 MS.
   */
  setHintDelayMS(rate: number): void {
    PrimaryTts.hint_delay_ms_ = 1000 / rate;
  }
}

TestImportManager.exportForTesting(PrimaryTts);
