// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from '//resources/js/load_time_data.js';

import {getCurrentSpeechRate, playFromSelectionTimeout} from '../common.js';
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
  onSpeechRateChange(): void;
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

  initializeSpeechTree(startingNodeId?: number) {
    if (startingNodeId && !this.model_.getFirstTextNode()) {
      this.model_.setFirstTextNode(startingNodeId);
    }

    const firstTextNode = this.model_.getFirstTextNode();
    if (!firstTextNode || this.isSpeechTreeInitialized()) {
      return;
    }

    // TODO: crbug.com/40927698 - There should be a way to use AXPosition so
    // that this step can be skipped.
    chrome.readingMode.initAxPositionWithNode(firstTextNode);
    this.model_.setIsSpeechTreeInitialized(true);
    chrome.readingMode.preprocessTextForSpeech();
  }

  // If the screen is locked during speech, we should stop speaking.
  onLockScreen() {
    if (this.isSpeechActive()) {
      this.stopSpeech(PauseActionSource.DEFAULT);
    }
  }

  onSpeechSettingsChange(): boolean {
    // Don't call stopSpeech() if the speech tree hasn't been initialized or
    // if speech hasn't been triggered yet.
    if (!this.isSpeechTreeInitialized() || !this.hasSpeechBeenTriggered()) {
      return false;
    }

    const playSpeechOnChange = this.isSpeechActive();

    // Cancel the queued up Utterance using the old speech settings
    this.stopSpeech(PauseActionSource.VOICE_SETTINGS_CHANGE);
    return playSpeechOnChange;
  }

  onHighlightGranularityChange(newGranularity: number) {
    chrome.readingMode.onHighlightGranularityChanged(newGranularity);

    // Rehighlight the new granularity.
    if (newGranularity !== chrome.readingMode.noHighlighting) {
      this.highlightCurrentGranularity(chrome.readingMode.getCurrentText());
    }

    // Log these highlight granularity changes when the phrase menu is shown.
    // (Toggles are already logged in the toolbar.)
    this.logger_.logHighlightGranularity(newGranularity);
  }

  onPlay() {
    this.model_.setPlaySessionStartTime(Date.now());
  }

  playNextGranularity() {
    this.setIsSpeechBeingRepositioned(true);

    this.speech_.cancel();
    this.highlighter_.resetPreviousHighlight();
    // Reset the word boundary index whenever we move the granularity position.
    this.wordBoundaries_.resetToDefaultState();
    chrome.readingMode.movePositionToNextGranularity();

    if (!this.highlightAndPlayMessage()) {
      this.onSpeechFinished();
    }
  }

  playPreviousGranularity() {
    this.setIsSpeechBeingRepositioned(true);
    this.speech_.cancel();
    // This must be called BEFORE calling
    // chrome.readingMode.movePositionToPreviousGranularity so we can accurately
    // determine what's currently being highlighted.
    this.highlighter_.removeCurrentHighlight();
    this.highlighter_.resetPreviousHighlight();
    // Reset the word boundary index whenever we move the granularity position.
    this.wordBoundaries_.resetToDefaultState();
    chrome.readingMode.movePositionToPreviousGranularity();

    if (!this.highlightAndPlayMessage(
            /*isInterrupted=*/ false,
            /*isMovingBackward=*/ true)) {
      this.onSpeechFinished();
    }
  }

  playFromSelection(startingNodeId: number, startingOffset: number) {
    // Iterate through the page from the beginning until we get to the
    // selection. This is so clicking previous works before the selection and
    // so the previous highlights are properly set.
    chrome.readingMode.resetGranularityIndex();
    // Iterate through the nodes asynchronously so that we can show the spinner
    // in the toolbar while we move up to the selection.
    setTimeout(() => {
      this.movePlaybackToNode(startingNodeId, startingOffset);
      // Set everything to previous and then play the next granularity, which
      // includes the selection.
      this.highlighter_.resetPreviousHighlight();
      if (!this.highlightAndPlayMessage()) {
        this.onSpeechFinished();
      }
    }, playFromSelectionTimeout);
  }

  highlightAndPlayInterruptedMessage(): boolean {
    return this.highlightAndPlayMessage(/* isInterrupted = */ true);
  }

  // Play text of these axNodeIds. When finished, read and highlight to read the
  // following text.
  // TODO: crbug.com/1474951 - Investigate using AXRange.GetText to get text
  // between start node / end nodes and their offsets.
  highlightAndPlayMessage(
      isInterrupted: boolean = false,
      isMovingBackward: boolean = false): boolean {
    // getCurrentText gets the AX Node IDs of text that should be spoken and
    // highlighted.
    const axNodeIds: number[] = chrome.readingMode.getCurrentText();

    // If there aren't any valid ax node ids returned by getCurrentText,
    // speech should stop.
    if (axNodeIds.length === 0) {
      return false;
    }

    if (this.nodeStore_.areNodesAllHidden(axNodeIds)) {
      return this.skipCurrentPosition_(isInterrupted, isMovingBackward);
    }

    const utteranceText = this.extractTextOf_(axNodeIds);
    // If node ids were returned but they don't exist in the Reading Mode panel,
    // there's been a mismatch between Reading Mode and Read Aloud. In this
    // case, we should move to the next Read Aloud node and attempt to continue
    // playing. TODO: crbug.com/332694565 - This fallback should never be
    // needed, but it is. Investigate root cause of Read Aloud / Reading Mode
    // mismatch. Additionally, the TTS engine may not like attempts to speak
    // whitespace, so move to the next utterance in that case.
    if (!utteranceText || utteranceText.trim().length === 0) {
      return this.skipCurrentPosition_(isInterrupted, isMovingBackward);
    }

    // If we're resuming a previously interrupted message, use word
    // boundaries (if available) to resume at the beginning of the current
    // word.
    if (isInterrupted && this.wordBoundaries_.hasBoundaries()) {
      const utteranceTextForWordBoundary =
          utteranceText.substring(this.wordBoundaries_.getResumeBoundary());
      // If we paused right at the end of the sentence, no need to speak the
      // ending punctuation.
      if (this.highlighter_.isInvalidHighlightForWordHighlighting(
              utteranceTextForWordBoundary.trim())) {
        this.wordBoundaries_.resetToDefaultState();
        return this.skipCurrentPosition_(isInterrupted, isMovingBackward);
      } else {
        this.playText_(utteranceTextForWordBoundary);
      }
    } else {
      this.playText_(utteranceText);
    }

    this.highlightCurrentGranularity(axNodeIds);
    return true;
  }

  private skipCurrentPosition_(
      isInterrupted: boolean, isMovingBackward: boolean): boolean {
    if (isMovingBackward) {
      chrome.readingMode.movePositionToPreviousGranularity();
    } else {
      chrome.readingMode.movePositionToNextGranularity();
    }
    return this.highlightAndPlayMessage(isInterrupted, isMovingBackward);
  }

  private playText_(utteranceText: string) {
    // This check is needed due limits of TTS audio for remote voices. See
    // crbug.com/1176078 for more details.
    // Since the TTS bug only impacts remote voices, no need to check for
    // maximum text length if we're using a local voice. If we do somehow
    // attempt to speak text that's too long, this will be able to be handled
    // by listening for a text-too-long error in message.onerror.
    const isTextTooLong = this.isTextTooLong(utteranceText);
    const endBoundary =
        this.getUtteranceEndBoundary(utteranceText, isTextTooLong);
    this.playTextWithBoundaries_(utteranceText, isTextTooLong, endBoundary);
  }

  private playTextWithBoundaries_(
      utteranceText: string, isTextTooLong: boolean, endBoundary: number) {
    const message =
        new SpeechSynthesisUtterance(utteranceText.substring(0, endBoundary));

    message.onerror = (error) => {
      this.handleSpeechSynthesisError_(error, utteranceText);
    };

    this.setOnBoundary(message);
    this.setOnSpeechSynthesisUtteranceStart(message);

    message.onend = () => {
      if (isTextTooLong) {
        // Since our previous utterance was too long, continue speaking pieces
        // of the current utterance until the utterance is complete. The
        // entire utterance is highlighted, so there's no need to update
        // highlighting until the utterance substring is an acceptable size.
        this.playText_(utteranceText.substring(endBoundary));
        return;
      }

      // Now that we've finiished reading this utterance, update the
      // Granularity state to point to the next one Reset the word boundary
      // index whenever we move the granularity position.
      this.wordBoundaries_.resetToDefaultState();
      chrome.readingMode.movePositionToNextGranularity();
      // Continue speaking with the next block of text.
      if (!this.highlightAndPlayMessage()) {
        this.onSpeechFinished();
      }
    };

    this.speakMessage(message);
  }

  private handleSpeechSynthesisError_(
      error: SpeechSynthesisErrorEvent, utteranceText: string) {
    // We can't be sure that the engine has loaded at this point, but
    // if there's an error, we want to ensure we keep the play buttons
    // to prevent trapping users in a state where they can no longer play
    // Read Aloud, as this is preferable to a long delay before speech
    // with no feedback.
    this.setEngineState(SpeechEngineState.LOADED);

    if (error.error === 'interrupted') {
      this.onSpeechInterrupted();
      return;
    }

    // Log a speech error. We aren't concerned with logging an interrupted
    // error, since that can be triggered from play / pause.
    this.logger_.logSpeechError(error.error);

    if (error.error === 'text-too-long') {
      // This is unlikely to happen, as the length limit on most voices
      // is quite long. However, if we do hit a limit, we should just use
      // the accessible text length boundaries to shorten the text. Even
      // if this gives a much smaller sentence than TTS would have supported,
      // this is still preferable to no speech.
      this.speech_.cancel();
      this.playTextWithBoundaries_(
          utteranceText, true,
          this.getUtteranceEndBoundary(utteranceText, true));
      return;
    }
    if (error.error === 'invalid-argument') {
      // invalid-argument can be triggered when the rate, pitch, or volume
      // is not supported by the synthesizer. Since we're only setting the
      // speech rate, update the speech rate to the WebSpeech default of 1.
      chrome.readingMode.onSpeechRateChange(1);
      this.listeners_.forEach(l => l.onSpeechRateChange());
      return;
    }

    // When we hit an error, stop speech to clear all utterances, update the
    // button state, and highlighting in order to give visual feedback that
    // something went wrong.
    // TODO: crbug.com/40927698 - Consider showing an error message.
    this.logger_.logSpeechStopSource(chrome.readingMode.engineErrorStopSource);
    this.stopSpeech(PauseActionSource.DEFAULT);

    // No appropriate voice is available for the language designated in
    // SpeechSynthesisUtterance lang.
    if (error.error === 'language-unavailable') {
      this.voicePackController_.onLanguageUnavailableError();
    }

    // The voice designated in SpeechSynthesisUtterance voice attribute
    // is not available.
    if (error.error === 'voice-unavailable') {
      this.voicePackController_.onVoiceUnavailableError();
    }
  }

  private extractTextOf_(axNodeIds: number[]): string {
    let utteranceText: string = '';
    for (const nodeId of axNodeIds) {
      const startIndex = chrome.readingMode.getCurrentTextStartIndex(nodeId);
      const endIndex = chrome.readingMode.getCurrentTextEndIndex(nodeId);
      const element = this.nodeStore_.getDomNode(nodeId);
      if (!element || startIndex < 0 || endIndex < 0) {
        continue;
      }
      const content = chrome.readingMode.getTextContent(nodeId).substring(
          startIndex, endIndex);
      if (content) {
        // Add all of the text from the current nodes into a single utterance.
        utteranceText += content;
      }
    }
    return utteranceText;
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
    this.model_.setFirstTextNode(null);
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
