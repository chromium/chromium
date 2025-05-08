// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from '//resources/js/load_time_data.js';

import {getCurrentSpeechRate} from '../common.js';
import {NodeStore} from '../node_store.js';
import {ReadAnythingLogger} from '../read_anything_logger.js';
import type {SpeechBrowserProxy} from '../speech_browser_proxy.js';
import {SpeechBrowserProxyImpl} from '../speech_browser_proxy.js';

import {ReadAloudHighlighter} from './highlighter.js';
import {PauseActionSource, SpeechEngineState, SpeechModel} from './speech_model.js';
import type {SpeechPlayingState} from './speech_model.js';
import {VoicePackController} from './voice_pack_controller.js';
import {WordBoundaries} from './word_boundaries.js';
import type {WordBoundaryState} from './word_boundaries.js';

// The maximum speech length that should be used with remote voices
// due to a TTS engine bug with voices timing out on too-long text.
export const MAX_SPEECH_LENGTH: number = 175;

export interface SpeechListener {
  onStop(): void;
  onIsSpeechActiveChange(): void;
  onIsAudioCurrentlyPlayingChange(): void;
  onEngineStateChange(): void;
  onPreviewVoicePlaying(): void;
}

export class SpeechController {
  private model_: SpeechModel = new SpeechModel();
  private speech_: SpeechBrowserProxy = SpeechBrowserProxyImpl.getInstance();
  private logger_: ReadAnythingLogger = ReadAnythingLogger.getInstance();
  private nodeStore_: NodeStore = NodeStore.getInstance();
  private voicePackController_: VoicePackController =
      VoicePackController.getInstance();
  private wordBoundaries_: WordBoundaries = WordBoundaries.getInstance();
  private highlighter_: ReadAloudHighlighter =
      ReadAloudHighlighter.getInstance();
  private listeners_: SpeechListener[] = [];

  constructor() {
    // Send over the initial state.
    this.clearReadAloudState();
    this.isSpeechActiveChanged(this.isSpeechActive());
  }

  addListener(listener: SpeechListener) {
    this.listeners_.push(listener);
  }

  getState(): SpeechPlayingState {
    return this.model_.getState();
  }

  setState(state: SpeechPlayingState) {
    if (state.isSpeechActive !== this.isSpeechActive()) {
      this.isSpeechActiveChanged(state.isSpeechActive);
    }
    if (state.isAudioCurrentlyPlaying !== this.isAudioCurrentlyPlaying()) {
      this.listeners_.forEach(l => l.onIsAudioCurrentlyPlayingChange());
    }

    this.model_.setState(state);
  }

  isSpeechActive(): boolean {
    return this.model_.isSpeechActive();
  }

  setIsSpeechActive(isSpeechActive: boolean) {
    if (isSpeechActive !== this.isSpeechActive()) {
      this.model_.setIsSpeechActive(isSpeechActive);
      this.isSpeechActiveChanged(isSpeechActive);
    }
  }

  isSpeechBeingRepositioned(): boolean {
    return this.model_.isSpeechBeingRepositioned();
  }

  setIsSpeechBeingRepositioned(isSpeechBeingRepositioned: boolean) {
    this.model_.setIsSpeechBeingRepositioned(isSpeechBeingRepositioned);
  }

  isAudioCurrentlyPlaying(): boolean {
    return this.model_.isAudioCurrentlyPlaying();
  }

  setIsAudioCurrentlyPlaying(isAudioCurrentlyPlaying: boolean) {
    if (isAudioCurrentlyPlaying !== this.isAudioCurrentlyPlaying()) {
      this.model_.setIsAudioCurrentlyPlaying(isAudioCurrentlyPlaying);
      this.listeners_.forEach(l => l.onIsAudioCurrentlyPlayingChange());
    }
  }

  isEngineLoaded(): boolean {
    return this.model_.getEngineState() === SpeechEngineState.LOADED;
  }

  setEngineState(state: SpeechEngineState) {
    if (state !== this.model_.getEngineState()) {
      this.model_.setEngineState(state);
      this.listeners_.forEach(l => l.onEngineStateChange());
    }
  }

  getPreviewVoicePlaying(): SpeechSynthesisVoice|null {
    return this.model_.getPreviewVoicePlaying();
  }

  setPreviewVoicePlaying(voice: SpeechSynthesisVoice|null) {
    if (voice !== this.model_.getPreviewVoicePlaying()) {
      this.model_.setPreviewVoicePlaying(voice);
      this.listeners_.forEach(l => l.onPreviewVoicePlaying());
    }
  }

  hasSpeechBeenTriggered(): boolean {
    return this.model_.hasSpeechBeenTriggered();
  }

  setHasSpeechBeenTriggered(hasSpeechBeenTriggered: boolean) {
    this.model_.setHasSpeechBeenTriggered(hasSpeechBeenTriggered);
  }

  isSpeechTreeInitialized(): boolean {
    return this.model_.isSpeechTreeInitialized();
  }

  getPauseSource(): PauseActionSource {
    return this.model_.getPauseSource();
  }

  isPausedFromButton(): boolean {
    return this.model_.getPauseSource() === PauseActionSource.BUTTON_CLICK;
  }

  reset() {
    this.model_.reset();
    this.listeners_.forEach(l => l.onIsSpeechActiveChange());
    this.listeners_.forEach(l => l.onIsAudioCurrentlyPlayingChange());
  }

  initializeSpeechTree(startingNodeId: number|null) {
    if (!startingNodeId || this.isSpeechTreeInitialized()) {
      return;
    }

    // TODO: crbug.com/40927698 - There should be a way to use AXPosition so
    // that this step can be skipped.
    chrome.readingMode.initAxPositionWithNode(startingNodeId);
    this.model_.setIsSpeechTreeInitialized(true);
    chrome.readingMode.preprocessTextForSpeech();
  }

  onPlay() {
    this.model_.setPlaySessionStartTime(Date.now());
  }

  stopSpeech(pauseSource: PauseActionSource) {
    this.setIsSpeechActive(false);
    this.setIsAudioCurrentlyPlaying(false);
    this.model_.setPauseSource(pauseSource);

    // Voice and speed changes take effect on the next call of synth.play(),
    // but not on .resume(). In order to be responsive to the user's settings
    // changes, we call synth.cancel() and synth.play(). However, we don't do
    // synth.cancel() and synth.play() when user clicks play/pause button,
    // because synth.cancel() and synth.play() plays from the beginning of the
    // current utterance, even if parts of it had been spoken already.
    // Therefore, when a user toggles the play/pause button, we call
    // synth.pause() and synth.resume() for speech to resume from where it left
    // off.
    if (this.isPausedFromButton()) {
      this.logSpeechPlaySession_();
      this.speech_.pause();
    } else {
      // Canceling clears all the Utterances that are queued up via synth.play()
      this.speech_.cancel();
    }

    this.listeners_.forEach(l => l.onStop());
  }

  setOnSpeechSynthesisUtteranceStart(message: SpeechSynthesisUtterance) {
    message.onstart = () => {
      // We've gotten the signal that the speech engine has started, therefore
      // we can enable the Read Aloud buttons.
      this.setEngineState(SpeechEngineState.LOADED);

      // Reset the isSpeechBeingRepositioned property after speech starts
      // after a next / previous button.
      this.setIsSpeechBeingRepositioned(false);
      this.setIsAudioCurrentlyPlaying(true);
    };
  }

  setOnBoundary(message: SpeechSynthesisUtterance) {
    message.onboundary = (event) => {
      // Some voices may give sentence boundaries, but we're only concerned
      // with word boundaries in boundary event because we're speaking text at
      // the sentence granularity level, so we'll retrieve these boundaries in
      // message.onEnd instead.
      if (event.name === 'word') {
        this.wordBoundaries_.updateBoundary(event.charIndex, event.charLength);

        // No need to update the highlight on word boundary events if
        // highlighting is off or if sentence highlighting is used.
        // Therefore, we don't need to pass in axIds because these are
        // calculated downstream.
        this.highlightCurrentGranularity(
            [], /* scrollIntoView= */ true,
            /*shouldUpdateSentenceHighlight= */ false);
      }
    };
  }

  speakMessage(message: SpeechSynthesisUtterance) {
    const voice = this.voicePackController_.getCurrentVoiceOrDefault();
    if (!voice) {
      // TODO: crbug.com/40927698 - Handle when no voices are available.
      return;
    }

    // This should only be false in tests where we can't properly construct an
    // actual SpeechSynthesisVoice object even though the test voices pass the
    // type checking of method signatures.
    if (voice instanceof SpeechSynthesisVoice) {
      message.voice = voice;
    }

    if (this.model_.getEngineState() === SpeechEngineState.NONE) {
      this.setEngineState(SpeechEngineState.LOADING);
    }

    this.speakWithDefaults_(message);
  }

  previewVoice(previewVoice: SpeechSynthesisVoice|null) {
    this.stopSpeech(PauseActionSource.VOICE_PREVIEW);

    // If there's no previewVoice, return after stopping the current preview
    if (!previewVoice) {
      this.setPreviewVoicePlaying(null);
      return;
    }

    const utterance = new SpeechSynthesisUtterance(
        loadTimeData.getString('readingModeVoicePreviewText'));
    // This should only be false in tests where we can't properly construct an
    // actual SpeechSynthesisVoice object even though the test voices pass the
    // type checking of method signatures.
    if (previewVoice instanceof SpeechSynthesisVoice) {
      utterance.voice = previewVoice;
    }

    utterance.onstart = () => {
      this.setPreviewVoicePlaying(previewVoice);
    };

    utterance.onend = () => {
      this.setPreviewVoicePlaying(null);
    };

    // TODO: crbug.com/40927698 - There should probably be more sophisticated
    // error handling for voice previews, but for now, simply setting the
    // preview voice to null should be sufficient to reset state if an error is
    // encountered during a preview.
    utterance.onerror = () => {
      this.setPreviewVoicePlaying(null);
    };

    this.speakWithDefaults_(utterance);
  }

  onSpeechInterrupted() {
    // SpeechSynthesis.cancel() was called, which could have originated
    // either within or outside of reading mode. If it originated from
    // within reading mode, we should do nothing. If it came from outside
    // of reading mode, we should stop speech to ensure that state
    // accuratively reflects the interrupted state.
    if (this.model_.isAudioCurrentlyPlaying() &&
        !this.model_.isSpeechBeingRepositioned()) {
      // If we're currently playing speech,  we're not currently in the
      // middle of a next / previous granularity update via button press,
      // and we receive an 'interrupted' error, it came from outside (e.g.
      // from opening another instance of reading mode), so we should
      // ensure speech state, including the play / pause button, is
      // updated.
      this.logger_.logSpeechStopSource(
          chrome.readingMode.engineInterruptStopSource);
      this.stopSpeech(PauseActionSource.ENGINE_INTERRUPT);
    }
  }

  onSpeechFinished() {
    this.clearReadAloudState();
    this.model_.setPauseSource(PauseActionSource.SPEECH_FINISHED);
    this.listeners_.forEach(l => l.onStop());
    this.logger_.logSpeechStopSource(
        chrome.readingMode.contentFinishedStopSource);
    this.logSpeechPlaySession_();
  }

  clearReadAloudState() {
    this.reset();
    this.highlighter_.clearHighlightFormatting();
    this.wordBoundaries_.resetToDefaultState();
  }

  setPreviousReadingPositionIfExists(
      previousWordBoundaryState: WordBoundaryState,
      previousSpeechPlayingState: SpeechPlayingState) {
    const lastPosition = this.model_.getLastPosition();
    if (!lastPosition) {
      return;
    }

    if (this.nodeStore_.getDomNode(lastPosition.nodeId)) {
      this.movePlaybackToNode(lastPosition.nodeId, lastPosition.offset);
      this.setState(previousSpeechPlayingState);
      this.wordBoundaries_.state = {...previousWordBoundaryState};
      // Since we're setting the reading position after a content update when
      // we're paused, redraw the highlight after moving the traversal state to
      // the right spot above.
      this.highlightCurrentGranularity(chrome.readingMode.getCurrentText());
    } else {
      this.model_.setLastPosition(null);
    }
  }

  movePlaybackToNode(nodeId: number, offset: number): void {
    let currentTextIds = chrome.readingMode.getCurrentText();
    let hasCurrentText = currentTextIds.length > 0;
    // Since a node could spread across multiple granularities, we use the
    // offset to determine if the selected text is in this granularity or if
    // we have to move to the next one.
    let startOfSelectionIsInCurrentText = currentTextIds.includes(nodeId) &&
        chrome.readingMode.getCurrentTextEndIndex(nodeId) > offset;
    while (hasCurrentText && !startOfSelectionIsInCurrentText) {
      this.highlightCurrentGranularity(
          currentTextIds, /*scrollIntoView=*/ false,
          /*shouldUpdateSentenceHighlight=*/ true,
          /*shouldSetLastReadingPos=*/ false);
      chrome.readingMode.movePositionToNextGranularity();
      currentTextIds = chrome.readingMode.getCurrentText();
      hasCurrentText = currentTextIds.length > 0;
      startOfSelectionIsInCurrentText = currentTextIds.includes(nodeId) &&
          chrome.readingMode.getCurrentTextEndIndex(nodeId) > offset;
    }
  }

  // Highlights or rehighlights the current granularity, sentence or word.
  highlightCurrentGranularity(
      axNodeIds: number[], scrollIntoView: boolean = true,
      shouldUpdateSentenceHighlight: boolean = true,
      shouldSetLastReadingPos: boolean = true) {
    if (shouldSetLastReadingPos && axNodeIds.length && axNodeIds[0]) {
      this.model_.setLastPosition({
        nodeId: axNodeIds[0],
        offset: chrome.readingMode.getCurrentTextStartIndex(axNodeIds[0]),
      });
    }
    this.highlighter_.highlightCurrentGranularity(
        axNodeIds, scrollIntoView, shouldUpdateSentenceHighlight);
  }

  isTextTooLong(text: string): boolean {
    return !this.voicePackController_.getCurrentVoice()?.localService &&
        text.length > MAX_SPEECH_LENGTH;
  }

  getUtteranceEndBoundary(text: string, isTextTooLong: boolean): number {
    return isTextTooLong ? this.getAccessibleTextLength_(text) : text.length;
  }

  // Gets the accessible text boundary for the given string.
  private getAccessibleTextLength_(text: string): number {
    // Splicing on commas won't work for all locales, but since this is a
    // simple strategy for splicing text in languages that do use commas
    // that reduces the need for calling getAccessibleBoundary.
    // TODO(crbug.com/40927698): Investigate if we can utilize comma splices
    // directly in the utils methods called by #getAccessibleBoundary.
    const lastCommaIndex =
        text.substring(0, MAX_SPEECH_LENGTH).lastIndexOf(', ');

    // To prevent infinite looping, only use the lastCommaIndex if it's not the
    // first character. Otherwise, use getAccessibleBoundary to prevent
    // repeatedly splicing on the first comma of the same substring.
    if (lastCommaIndex > 0) {
      return lastCommaIndex;
    }

    // TODO: crbug.com/40927698 - getAccessibleBoundary breaks on the nearest
    // word boundary, but if there's some type of punctuation (such as a comma),
    // it would be preferable to break on the punctuation so the pause in
    // speech sounds more natural.
    return chrome.readingMode.getAccessibleBoundary(text, MAX_SPEECH_LENGTH);
  }

  private isSpeechActiveChanged(isSpeechActive: boolean) {
    this.listeners_.forEach(l => l.onIsSpeechActiveChange());
    chrome.readingMode.onSpeechPlayingStateChanged(isSpeechActive);
  }

  private speakWithDefaults_(message: SpeechSynthesisUtterance) {
    message.lang = chrome.readingMode.baseLanguageForSpeech;
    message.rate = getCurrentSpeechRate();
    this.speech_.speak(message);
  }

  private logSpeechPlaySession_() {
    const startTime = this.model_.getPlaySessionStartTime();
    if (startTime) {
      this.logger_.logSpeechPlaySession(
          startTime, this.voicePackController_.getCurrentVoice());
      this.model_.setPlaySessionStartTime(null);
    }
  }

  static getInstance(): SpeechController {
    return instance || (instance = new SpeechController());
  }

  static setInstance(obj: SpeechController) {
    instance = obj;
  }
}

let instance: SpeechController|null = null;
