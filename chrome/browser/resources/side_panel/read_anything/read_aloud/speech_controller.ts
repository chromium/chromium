// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from '//resources/js/load_time_data.js';

import {NodeStore} from '../content/node_store.js';
import {SelectionController} from '../content/selection_controller.js';
import {getWordCount, playFromSelectionTimeout} from '../shared/common.js';
import {ReadAnythingLogger} from '../shared/read_anything_logger.js';

import {ReadAloudHighlighter} from './highlighter.js';
import {getReadAloudModel} from './read_aloud_model_browser_proxy.js';
import type {ReadAloudModelBrowserProxy} from './read_aloud_model_browser_proxy.js';
import {ReadAloudNode} from './read_aloud_types.js';
import type {Segment} from './read_aloud_types.js';
import type {SpeechBrowserProxy} from './speech_browser_proxy.js';
import {SpeechBrowserProxyImpl} from './speech_browser_proxy.js';
import {PauseActionSource, SpeechEngineState, SpeechModel} from './speech_model.js';
import type {SpeechPlayingState} from './speech_model.js';
import {getCurrentSpeechRate, isInvalidHighlightForWordHighlighting} from './speech_presentation_rules.js';
import {VoiceLanguageController} from './voice_language_controller.js';
import {WordBoundaries} from './word_boundaries.js';

// The maximum speech length that should be used with remote voices
// due to a TTS engine bug with voices timing out on too-long text.
export const MAX_SPEECH_LENGTH: number = 175;

export interface SpeechListener {
  onIsSpeechActiveChange(): void;
  onIsAudioCurrentlyPlayingChange(): void;
  onEngineStateChange(): void;
  onPreviewVoicePlaying(): void;
  onPlayingFromSelection(): void;
}

export class SpeechController {
  private model_: SpeechModel = new SpeechModel();
  private speech_: SpeechBrowserProxy = SpeechBrowserProxyImpl.getInstance();
  private logger_: ReadAnythingLogger = ReadAnythingLogger.getInstance();
  private nodeStore_: NodeStore = NodeStore.getInstance();
  private voiceLanguageController_: VoiceLanguageController =
      VoiceLanguageController.getInstance();
  private wordBoundaries_: WordBoundaries = WordBoundaries.getInstance();
  private highlighter_: ReadAloudHighlighter =
      ReadAloudHighlighter.getInstance();
  private selectionController_: SelectionController =
      SelectionController.getInstance();
  private listeners_: SpeechListener[] = [];
  private readAloudModel_: ReadAloudModelBrowserProxy = getReadAloudModel();

  constructor() {
    // Send over the initial state.
    this.clearReadAloudState();
    this.isSpeechActiveChanged_(this.isSpeechActive());
  }

  resetForNewContent() {
    if (chrome.readingMode.isTsTextSegmentationEnabled) {
      // Reset the read aloud model because there's new content.
      this.readAloudModel_.resetModel?.();
    }

    this.clearReadAloudState();
  }

  addListener(listener: SpeechListener) {
    this.listeners_.push(listener);
  }

  private setState_(state: SpeechPlayingState) {
    const wasSpeechActive = this.isSpeechActive();
    const wasAudioPlaying = this.isAudioCurrentlyPlaying();
    this.model_.setState(state);
    if (state.isSpeechActive !== wasSpeechActive) {
      this.isSpeechActiveChanged_(state.isSpeechActive);
    }
    if (state.isAudioCurrentlyPlaying !== wasAudioPlaying) {
      this.isAudioCurrentlyPlayingChanged_(state.isAudioCurrentlyPlaying);
    }
  }

  isSpeechActive(): boolean {
    return this.model_.isSpeechActive();
  }

  private setIsSpeechActive_(isSpeechActive: boolean) {
    if (isSpeechActive !== this.isSpeechActive()) {
      this.model_.setIsSpeechActive(isSpeechActive);
      this.isSpeechActiveChanged_(isSpeechActive);
    }
  }

  isSpeechBeingRepositioned(): boolean {
    return this.model_.isSpeechBeingRepositioned();
  }

  isAudioCurrentlyPlaying(): boolean {
    return this.model_.isAudioCurrentlyPlaying();
  }

  private setIsAudioCurrentlyPlaying_(isAudioCurrentlyPlaying: boolean) {
    if (isAudioCurrentlyPlaying !== this.isAudioCurrentlyPlaying()) {
      this.model_.setIsAudioCurrentlyPlaying(isAudioCurrentlyPlaying);
      this.isAudioCurrentlyPlayingChanged_(isAudioCurrentlyPlaying);
    }
  }

  isEngineLoaded(): boolean {
    return this.model_.getEngineState() === SpeechEngineState.LOADED;
  }

  private setEngineState_(state: SpeechEngineState) {
    if (state !== this.model_.getEngineState()) {
      this.model_.setEngineState(state);
      this.listeners_.forEach(l => l.onEngineStateChange());
    }
  }

  getPreviewVoicePlaying(): SpeechSynthesisVoice|null {
    return this.model_.getPreviewVoicePlaying();
  }

  private setPreviewVoicePlaying_(voice: SpeechSynthesisVoice|null) {
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
    return this.readAloudModel_.isInitialized();
  }

  isPausedFromButton(): boolean {
    return this.model_.getPauseSource() === PauseActionSource.BUTTON_CLICK;
  }

  isTemporaryPause(): boolean {
    const source = this.model_.getPauseSource();
    return (source === PauseActionSource.VOICE_PREVIEW) ||
        (source === PauseActionSource.VOICE_SETTINGS_CHANGE);
  }

  initializeSpeechTree(context?: Node) {
    if (context && !this.model_.getContextNode()) {
      this.model_.setContextNode(context);
    }

    const contextNode = this.model_.getContextNode();
    if (!contextNode || this.isSpeechTreeInitialized()) {
      return;
    }

    // TODO: crbug.com/40927698 - This step should be skipped on migrating to
    // a non-AXPosition-based text segmentation strategy.
    this.readAloudModel_.init(contextNode);
  }

  onSelectionChange() {
    this.highlighter_.clearHighlightFormatting();
  }

  // If the screen is locked during speech, we should stop speaking.
  onLockScreen() {
    if (this.isSpeechActive()) {
      this.stopSpeech_(PauseActionSource.DEFAULT);
    }
  }

  onTabMuteStateChange(muted: boolean) {
    this.model_.setVolume(muted ? 0.0 : 1.0);
    this.onSpeechSettingsChange();
  }

  onVoiceSelected(selectedVoice: SpeechSynthesisVoice) {
    const currentVoice = this.voiceLanguageController_.getCurrentVoice();
    this.voiceLanguageController_.setUserPreferredVoice(selectedVoice);

    // If the locales are identical, the voices are likely from the same
    // TTS engine, therefore, we don't need to reset the word boundary state.
    if (currentVoice?.lang.toLowerCase() !== selectedVoice.lang.toLowerCase()) {
      this.wordBoundaries_.setNotSupported();
    }
  }

  onSpeechSettingsChange(): void {
    // Don't call stopSpeech() if the speech tree hasn't been initialized or
    // if speech hasn't been triggered yet.
    if (!this.isSpeechTreeInitialized() || !this.hasSpeechBeenTriggered()) {
      return;
    }

    const resumeSpeechOnChange = this.isSpeechActive();

    // Cancel the queued up Utterance using the old speech settings
    this.stopSpeech_(PauseActionSource.VOICE_SETTINGS_CHANGE);
    if (resumeSpeechOnChange) {
      this.resumeSpeech_();
    }
  }

  onHighlightGranularityChange(newGranularity: number) {
    // Rehighlight the new granularity.
    if (newGranularity !== chrome.readingMode.noHighlighting) {
      this.highlightCurrentGranularity_(
          this.readAloudModel_.getCurrentTextSegments());
    }
  }

  onPlayPauseKeyPress(context: HTMLElement|null) {
    if (this.isSpeechActive()) {
      this.logger_.logSpeechStopSource(
          chrome.readingMode.keyboardShortcutStopSource);
    }
    this.onPlayPauseToggle(context);
  }

  onPlayPauseToggle(context: HTMLElement|null) {
    if (this.isSpeechActive()) {
      this.stopSpeech_(PauseActionSource.BUTTON_CLICK);
    } else {
      this.playSpeech_(context);
      this.model_.setPlaySessionStartTime(Date.now());
    }
  }

  private playSpeech_(context: HTMLElement|null) {
    if (this.hasSpeechBeenTriggered() && !this.isSpeechActive()) {
      this.resumeSpeech_();
    } else {
      this.playSpeechForTheFirstTime_(context);
    }
  }

  onNextGranularityClick() {
    this.model_.setIsSpeechBeingRepositioned(true);
    this.moveToNextGranularity_();
    // Reset the word boundary index whenever we move the granularity position.
    this.wordBoundaries_.resetToDefaultState();
    if (!this.highlightAndPlayMessage_()) {
      this.onSpeechFinished_();
    }
  }

  // Prefer calling this rather than moveSpeechForward directly so
  // that the highlighter is always informed of the change.
  private moveToNextGranularity_() {
    this.highlighter_.onWillMoveToNextGranularity(
        this.readAloudModel_.getCurrentTextSegments());
    this.readAloudModel_.moveSpeechForward();
  }

  onPreviousGranularityClick() {
    this.model_.setIsSpeechBeingRepositioned(true);
    this.moveToPreviousGranularity_();
    this.wordBoundaries_.resetToDefaultState();
    if (!this.highlightAndPlayMessage_(
            /*isInterrupted=*/ false,
            /*isMovingBackward=*/ true)) {
      this.onSpeechFinished_();
    }
  }

  // Prefer calling this rather than moveSpeechBackward directly so
  // that the highlighter is always informed of the change.
  private moveToPreviousGranularity_() {
    // This must be called BEFORE calling
    // moveSpeechBackwards so we can accurately
    // determine what's currently being highlighted.
    this.highlighter_.onWillMoveToPreviousGranularity();
    this.readAloudModel_.moveSpeechBackwards();
  }

  private resumeSpeech_() {
    let playedFromSelection = false;
    if (this.selectionController_.hasSelection()) {
      this.wordBoundaries_.resetToDefaultState();
      playedFromSelection = this.playFromSelection_();
    }

    if (!playedFromSelection) {
      if (this.isPausedFromButton() && !this.wordBoundaries_.hasBoundaries()) {
        // If word boundaries aren't supported for the given voice, we should
        // still continue to use synth.resume, as this is preferable to
        // restarting the current message.
        this.speech_.resume();
      } else {
        this.highlighter_.restorePreviousHighlighting();
        if (!this.highlightAndPlayInterruptedMessage_()) {
          // Ensure we're updating Read Aloud state if there's no text to
          // speak.
          this.onSpeechFinished_();
        }
      }
    }

    this.setIsSpeechActive_(true);
    this.model_.setIsSpeechBeingRepositioned(false);

    // If the current read highlight has been cleared from a call to
    // updateContent, such as via a preference change, rehighlight the nodes
    // after a pause.
    if (!playedFromSelection) {
      this.highlightCurrentGranularity_(
          this.readAloudModel_.getCurrentTextSegments());
    }
  }

  private playSpeechForTheFirstTime_(context: HTMLElement|null) {
    if (!context || !context.textContent) {
      return;
    }

    // Log that we're playing speech on a new page, but not when resuming.
    // This helps us compare how many reading mode pages are opened with
    // speech played and without speech played. Counting resumes would
    // inflate the speech played number.
    this.logger_.logNewPage(/*speechPlayed=*/ true);
    this.setIsSpeechActive_(true);
    this.setHasSpeechBeenTriggered(true);
    this.model_.setIsSpeechBeingRepositioned(false);

    // When the TS segmentation flag is enabled, playFromSelection_ needs to
    // called after initializeSpeechTree. While this change is probably okay
    // to introduce for the non-TS segmentation flag case, the original
    // order is maintained when the flag is disabled to reduce the risk of
    // introducing unexpected bugs to the V8 segmentation method.
    if (!chrome.readingMode.isTsTextSegmentationEnabled) {
      const playedFromSelection = this.playFromSelection_();
      if (playedFromSelection) {
        return;
      }
    }

    if (chrome.readingMode.isTsTextSegmentationEnabled) {
      // TODO: crbug.com/440400392- The speech tree should also be initialized
      // before the play button is pressed.
      this.initializeSpeechTree(context);
    } else {
      this.initializeSpeechTree();
    }

    if (chrome.readingMode.isTsTextSegmentationEnabled) {
      const playedFromSelection = this.playFromSelection_();
      if (playedFromSelection) {
        return;
      }
    }
    if (this.isSpeechTreeInitialized() && !this.highlightAndPlayMessage_()) {
      // Ensure we're updating Read Aloud state if there's no text to speak.
      this.onSpeechFinished_();
    }
  }

  private playFromSelection_(): boolean {
    if (!this.isSpeechTreeInitialized() ||
        !this.selectionController_.hasSelection()) {
      return false;
    }

    const selectionStart = this.selectionController_.getCurrentSelectionStart();
    const startingNodeId = selectionStart.nodeId;
    if (!startingNodeId) {
      return false;
    }

    this.listeners_.forEach(l => l.onPlayingFromSelection());
    // Iterate through the page from the beginning until we get to the
    // selection. This is so clicking previous works before the selection and
    // so the previous highlights are properly set.
    this.readAloudModel_.resetSpeechToBeginning();
    this.highlighter_.reset();
    // Iterate through the nodes asynchronously so that we can show the spinner
    // in the toolbar while we move up to the selection.
    setTimeout(() => {
      const domNode = this.nodeStore_.getDomNode(startingNodeId);
      if (!domNode) {
        return;
      }
      const readAloudNode = ReadAloudNode.create(domNode);
      if (!readAloudNode) {
        return;
      }
      this.movePlaybackToNode_(readAloudNode, selectionStart.offset);
      // Play the next granularity, which includes the selection.
      if (!this.highlightAndPlayMessage_()) {
        this.onSpeechFinished_();
      }
    }, playFromSelectionTimeout);
    return true;
  }

  private highlightAndPlayInterruptedMessage_(): boolean {
    return this.highlightAndPlayMessage_(/* isInterrupted = */ true);
  }

  // Play text of these axNodeIds. When finished, read and highlight to read the
  // following text.
  // TODO: crbug.com/1474951 - Investigate using AXRange.GetText to get text
  // between start node / end nodes and their offsets.
  private highlightAndPlayMessage_(
      isInterrupted: boolean = false,
      isMovingBackward: boolean = false): boolean {
    const segments: Segment[] = this.readAloudModel_.getCurrentTextSegments();

    // If there aren't any valid ax node ids returned by getCurrentText,
    // speech should stop.
    if (segments.length === 0) {
      return false;
    }

    // If node ids were returned but they don't exist in the Reading Mode panel,
    // there's been a mismatch between Reading Mode and Read Aloud. In this
    // case, we should move to the next Read Aloud node and attempt to continue
    // playing. TODO: crbug.com/332694565 - This fallback should never be
    // needed, but it is. Investigate root cause of Read Aloud / Reading Mode
    // mismatch. Additionally, the TTS engine may not like attempts to speak
    // whitespace, so move to the next utterance in that case.
    const nodes: ReadAloudNode[] = segments.map(segment => segment.node);
    if (!this.nodeStore_.hasAnyNode(nodes) ||
        this.nodeStore_.areNodesAllHidden(nodes)) {
      return this.skipCurrentPosition_(isInterrupted, isMovingBackward);
    }

    const utteranceText = this.readAloudModel_.getCurrentTextContent();
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
      if (isInvalidHighlightForWordHighlighting(
              utteranceTextForWordBoundary.trim())) {
        this.wordBoundaries_.resetToDefaultState();
        return this.skipCurrentPosition_(isInterrupted, isMovingBackward);
      } else {
        this.playText_(utteranceTextForWordBoundary);
      }
    } else {
      this.playText_(utteranceText);
    }

    this.highlightCurrentGranularity_(segments);
    return true;
  }

  private skipCurrentPosition_(
      isInterrupted: boolean, isMovingBackward: boolean): boolean {
    if (isMovingBackward) {
      this.moveToPreviousGranularity_();
    } else {
      this.moveToNextGranularity_();
    }
    return this.highlightAndPlayMessage_(isInterrupted, isMovingBackward);
  }

  private playText_(utteranceText: string) {
    // This check is needed due limits of TTS audio for remote voices. See
    // crbug.com/1176078 for more details.
    // Since the TTS bug only impacts remote voices, no need to check for
    // maximum text length if we're using a local voice. If we do somehow
    // attempt to speak text that's too long, this will be able to be handled
    // by listening for a text-too-long error in message.onerror.
    const isTextTooLong = this.isTextTooLong_(utteranceText);
    const textToPlay = this.getUtteranceText_(utteranceText, isTextTooLong);
    this.playTextWithBoundaries_(utteranceText, isTextTooLong, textToPlay);
  }

  private playTextWithBoundaries_(
      utteranceText: string, isUtteranceTextTooLong: boolean,
      textToPlay: string) {
    const message = new SpeechSynthesisUtterance(textToPlay);

    message.onerror = (error) => {
      this.handleSpeechSynthesisError_(error, utteranceText);
    };

    this.setOnBoundary_(message);
    this.setOnSpeechSynthesisUtteranceStart_(message);

    const text = message.text;
    message.onend = () => {
      if (isUtteranceTextTooLong) {
        // Since our previous utterance was too long, continue speaking pieces
        // of the current utterance until the utterance is complete. The
        // entire utterance is highlighted, so there's no need to update
        // highlighting until the utterance substring is an acceptable size.
        const remainingText = utteranceText.substring(textToPlay.length);
        this.playText_(remainingText);
        return;
      }

      this.countWordsHeardIfNeeded(text);
      // Now that we've finiished reading this utterance, update the
      // Granularity state to point to the next one Reset the word
      // boundary index whenever we move the granularity position.
      this.wordBoundaries_.resetToDefaultState();
      this.moveToNextGranularity_();
      // Continue speaking with the next block of text.
      if (!this.highlightAndPlayMessage_()) {
        this.onSpeechFinished_();
      }
    };

    this.speakMessage_(message);
  }

  // If word boundaries are not supported, use string parsing to determine how
  // many words were heard.
  private countWordsHeardIfNeeded(text: string) {
    if (this.wordBoundaries_.notSupported()) {
      const wordCount = getWordCount(text);
      this.model_.setWordsHeard(this.model_.getWordsHeard() + wordCount);
      chrome.readingMode.updateWordsHeard(this.model_.getWordsHeard());
    }
  }

  private handleSpeechSynthesisError_(
      error: SpeechSynthesisErrorEvent, utteranceText: string) {
    // We can't be sure that the engine has loaded at this point, but
    // if there's an error, we want to ensure we keep the play buttons
    // to prevent trapping users in a state where they can no longer play
    // Read Aloud, as this is preferable to a long delay before speech
    // with no feedback.
    this.setEngineState_(SpeechEngineState.LOADED);

    if (error.error === 'interrupted') {
      this.onSpeechInterrupted_();
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
      this.playTextWithBoundaries_(
          utteranceText, true, this.getUtteranceText_(utteranceText, true));
      return;
    }
    if (error.error === 'invalid-argument') {
      // invalid-argument can be triggered when the rate, pitch, or volume
      // is not supported by the synthesizer. Since we're only setting the
      // speech rate, update the speech rate to the WebSpeech default of 1.
      chrome.readingMode.onSpeechRateChange(1);
      this.onSpeechSettingsChange();
      return;
    }

    // When we hit an error, stop speech to clear all utterances, update the
    // button state, and highlighting in order to give visual feedback that
    // something went wrong.
    // TODO: crbug.com/40927698 - Consider showing an error message.
    this.logger_.logSpeechStopSource(chrome.readingMode.engineErrorStopSource);
    this.stopSpeech_(PauseActionSource.DEFAULT);

    // No appropriate voice is available for the language designated in
    // SpeechSynthesisUtterance lang.
    if (error.error === 'language-unavailable') {
      this.voiceLanguageController_.onLanguageUnavailableError();
    }

    // The voice designated in SpeechSynthesisUtterance voice attribute
    // is not available.
    if (error.error === 'voice-unavailable') {
      this.voiceLanguageController_.onVoiceUnavailableError();
    }
  }

  private stopSpeech_(pauseSource: PauseActionSource) {
    // Pause source needs to be set before updating isSpeechActive so that
    // listeners get the correct source when listening for isSpeechActive
    // changes. Only update the pause source to the one that actually stopped
    // speech.
    if (this.isSpeechActive()) {
      this.model_.setPauseSource(pauseSource);
    }
    this.setIsSpeechActive_(false);
    this.setIsAudioCurrentlyPlaying_(false);

    // Voice and speed changes take effect on the next call of synth.play(),
    // but not on .resume(). In order to be responsive to the user's settings
    // changes, we call synth.cancel() and synth.play(). However, we don't do
    // synth.cancel() and synth.play() when user clicks play/pause button,
    // because synth.cancel() and synth.play() plays from the beginning of the
    // current utterance, even if parts of it had been spoken already.
    // Therefore, when a user toggles the play/pause button, we call
    // synth.pause() and synth.resume() for speech to resume from where it left
    // off.
    // If we're stopping because of an interrupt, then speech was already
    // canceled, so we shouldn't cancel again, in case we are queuing up speech
    // in another tab.
    if (this.isPausedFromButton()) {
      this.logSpeechPlaySession_();
      this.speech_.pause();
    } else if (pauseSource !== PauseActionSource.ENGINE_INTERRUPT) {
      // Canceling clears all the Utterances that are queued up via synth.play()
      this.speech_.cancel();
    }
  }

  private setOnSpeechSynthesisUtteranceStart_(
      message: SpeechSynthesisUtterance) {
    message.onstart = () => {
      // We've gotten the signal that the speech engine has started, therefore
      // we can enable the Read Aloud buttons.
      this.setEngineState_(SpeechEngineState.LOADED);

      // Reset the isSpeechBeingRepositioned property after speech starts
      // after a next / previous button.
      this.model_.setIsSpeechBeingRepositioned(false);
      this.setIsAudioCurrentlyPlaying_(true);
    };
  }

  private setOnBoundary_(message: SpeechSynthesisUtterance) {
    message.onboundary = (event) => {
      // Some voices may give sentence boundaries, but we're only concerned
      // with word boundaries in boundary event because we're speaking text at
      // the sentence granularity level, so we'll retrieve these boundaries in
      // message.onEnd instead.
      if (event.name === 'word') {
        const text = message.text;
        const end = event.charIndex + (event.charLength || text.length);
        const possibleWord = text.substring(event.charIndex, end).trim();
        if (!isInvalidHighlightForWordHighlighting(possibleWord)) {
          // TODO(crbug.com/c/372890165): Consider adding a heuristic to ensure
          // we aren't counting the same word multiple times, if the TTS engine
          // word boundaries are inaccurate.
          this.model_.incrementWordsHeard();
          // TODO(crbug.com/c/372890165): Consider using words heard to better
          // estimate words seen.
          chrome.readingMode.updateWordsHeard(this.model_.getWordsHeard());
        }

        this.wordBoundaries_.updateBoundary(event.charIndex, event.charLength);

        // No need to update the highlight on word boundary events if
        // highlighting is off or if sentence highlighting is used.
        // Therefore, we don't need to pass in axIds because these are
        // calculated downstream.
        this.highlightCurrentGranularity_(
            [], /* scrollIntoView= */ true,
            /*shouldUpdateSentenceHighlight= */ false);
      }
    };
  }

  private speakMessage_(message: SpeechSynthesisUtterance) {
    const voice = this.voiceLanguageController_.getCurrentVoiceOrDefault();
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
      this.setEngineState_(SpeechEngineState.LOADING);
    }

    this.speakWithDefaults_(message);
  }

  previewVoice(previewVoice: SpeechSynthesisVoice|null) {
    this.stopSpeech_(PauseActionSource.VOICE_PREVIEW);

    // If there's no previewVoice, return after stopping the current preview
    if (!previewVoice) {
      this.setPreviewVoicePlaying_(null);
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
      this.setPreviewVoicePlaying_(previewVoice);
    };

    utterance.onend = () => {
      this.setPreviewVoicePlaying_(null);
    };

    // TODO: crbug.com/40927698 - There should probably be more sophisticated
    // error handling for voice previews, but for now, simply setting the
    // preview voice to null should be sufficient to reset state if an error is
    // encountered during a preview.
    utterance.onerror = () => {
      this.setPreviewVoicePlaying_(null);
    };

    this.speakWithDefaults_(utterance);
  }

  onVoiceMenuOpen() {
    this.model_.setResumeSpeechOnVoiceMenuClose(this.isSpeechActive());
  }

  onVoiceMenuClose() {
    // TODO: crbug.com/323912186 - Handle when menu is closed mid-preview and
    // the user presses play/pause button.
    if (!this.isSpeechActive() &&
        this.model_.getResumeSpeechOnVoiceMenuClose()) {
      this.resumeSpeech_();
    }
  }

  private onSpeechInterrupted_() {
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
      this.stopSpeech_(PauseActionSource.ENGINE_INTERRUPT);
    }
  }

  private onSpeechFinished_() {
    this.clearReadAloudState();
    this.readAloudModel_.resetSpeechToBeginning();

    this.model_.setPauseSource(PauseActionSource.SPEECH_FINISHED);
    this.logger_.logSpeechStopSource(
        chrome.readingMode.contentFinishedStopSource);
    this.logSpeechPlaySession_();
  }

  onScroll() {
    // If the reading mode panel was scrolled while read aloud is speaking,
    // we should disable autoscroll if the highlights are no longer visible,
    // and we should re-enable autoscroll if the highlights are now
    // visible.
    if (this.isSpeechActive()) {
      this.highlighter_.updateAutoScroll();
    }
  }

  clearReadAloudState() {
    this.speech_.cancel();
    this.highlighter_.reset();
    this.wordBoundaries_.resetToDefaultState();

    const speechPlayingState = {
      isSpeechActive: false,
      pauseSource: PauseActionSource.DEFAULT,
      isAudioCurrentlyPlaying: false,
      hasSpeechBeenTriggered: false,
      isSpeechBeingRepositioned: false,
    };
    this.setState_(speechPlayingState);
    this.setPreviewVoicePlaying_(null);
    this.model_.setContextNode(null);
    this.model_.setResumeSpeechOnVoiceMenuClose(false);
    this.model_.setWordsHeard(0);
  }

  saveReadAloudState() {
    this.model_.setSavedSpeechPlayingState({...this.model_.getState()});
    this.model_.setSavedWordBoundaryState({...this.wordBoundaries_.state});
  }

  setPreviousReadingPositionIfExists(): boolean {
    const savedSpeechPlayingState = this.model_.getSavedSpeechPlayingState();
    const savedWordBoundaryState = this.model_.getSavedWordBoundaryState();
    const lastPosition = this.model_.getLastPosition();
    this.model_.setSavedSpeechPlayingState(null);
    this.model_.setSavedWordBoundaryState(null);
    if (!savedWordBoundaryState || !savedSpeechPlayingState ||
        !savedSpeechPlayingState.hasSpeechBeenTriggered || !lastPosition) {
      return false;
    }

    const lastNode = lastPosition.node;
    if (lastNode.domNode()) {
      this.movePlaybackToNode_(lastNode, lastPosition.offset);
      this.setState_(savedSpeechPlayingState);
      this.wordBoundaries_.state = savedWordBoundaryState;
      // Since we're setting the reading position after a content update when
      // we're paused, redraw the highlight after moving the traversal state to
      // the right spot above.
      this.highlightCurrentGranularity_(
          this.readAloudModel_.getCurrentTextSegments());
      return true;
    } else {
      this.model_.setLastPosition(null);
      return false;
    }
  }

  private movePlaybackToNode_(node: ReadAloudNode, offset: number): void {
    let currentSegments: Segment[] =
        this.readAloudModel_.getCurrentTextSegments();
    let hasCurrentText = currentSegments.length > 0;
    // Since a node could spread across multiple granularities, we use the
    // offset to determine if the selected text is in this granularity or if
    // we have to move to the next one.
    let foundSegment = this.findSegment_(currentSegments, node, offset);
    while (hasCurrentText && !foundSegment) {
      this.highlightCurrentGranularity_(
          currentSegments, /*scrollIntoView=*/ false,
          /*shouldUpdateSentenceHighlight=*/ true,
          /*shouldSetLastReadingPos=*/ false);
      this.moveToNextGranularity_();

      currentSegments = this.readAloudModel_.getCurrentTextSegments();
      hasCurrentText = currentSegments.length > 0;
      foundSegment = this.findSegment_(currentSegments, node, offset);
    }
  }

  private findSegment_(
      segments: Segment[], node: ReadAloudNode, offset: number): Segment
      |undefined {
    // When the TsTextSegmentation flag is enabled, findSegment_ should count a
    // match if the selection node contains the read aloud node (i.e. the read
    // aloud node is a child of the selection node) - otherwise there
    // won't be a match on the first run of playFromSelection()
    if (!chrome.readingMode.isTsTextSegmentationEnabled) {
      return segments.find(
          segment => segment.node.equals(node) &&
              (segment.start + segment.length > offset));
    }

    const selectedDomNode = node.domNode();
    if (!selectedDomNode) {
      return undefined;
    }
    return segments.find(segment => {
      const segmentDomNode = segment.node.domNode();
      if (!segmentDomNode) {
        return false;
      }

      if (segment.node.equals(node)) {
        return (segment.start + segment.length > offset);
      }

      if (selectedDomNode.contains(segmentDomNode)) {
        return true;
      }

      return false;
    });
  }

  // Highlights or rehighlights the current granularity, sentence or word.
  private highlightCurrentGranularity_(
      segments: Segment[], scrollIntoView: boolean = true,
      shouldUpdateSentenceHighlight: boolean = true,
      shouldSetLastReadingPos: boolean = true) {
    if (shouldSetLastReadingPos && segments.length && segments[0]) {
      this.model_.setLastPosition({
        node: segments[0].node,
        offset: segments[0].start,
      });
    }
    this.highlighter_.highlightCurrentGranularity(
        segments, scrollIntoView, shouldUpdateSentenceHighlight);
  }

  private isTextTooLong_(text: string): boolean {
    return !this.voiceLanguageController_.getCurrentVoice()?.localService &&
        text.length > MAX_SPEECH_LENGTH;
  }

  private getUtteranceText_(text: string, isTextTooLong: boolean): string {
    // If the text is not considered too long, don't get its accessible
    // utterance to avoid shortening the utterance unnecessarily.
    return isTextTooLong ? this.getAccessibleUtterance_(text) : text;
  }

  // Gets the accessible utterance for the given string.
  private getAccessibleUtterance_(text: string): string {
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
      return text.substring(0, lastCommaIndex);
    }

    // TODO: crbug.com/40927698 - getAccessibleBoundary breaks on the nearest
    // word boundary, but if there's some type of punctuation (such as a comma),
    // it would be preferable to break on the punctuation so the pause in
    // speech sounds more natural.
    return this.readAloudModel_.getAccessibleText(text, MAX_SPEECH_LENGTH);
  }

  private isSpeechActiveChanged_(isSpeechActive: boolean) {
    this.listeners_.forEach(l => l.onIsSpeechActiveChange());
    chrome.readingMode.onIsSpeechActiveChanged(isSpeechActive);
  }

  private isAudioCurrentlyPlayingChanged_(isAudioCurrentlyPlaying: boolean) {
    this.listeners_.forEach(l => l.onIsAudioCurrentlyPlayingChange());
    chrome.readingMode.onIsAudioCurrentlyPlayingChanged(
        isAudioCurrentlyPlaying);
  }

  private speakWithDefaults_(message: SpeechSynthesisUtterance) {
    message.volume = this.model_.getVolume();
    message.lang = chrome.readingMode.baseLanguageForSpeech;
    message.rate = getCurrentSpeechRate();
    // Cancel any pending utterances that may be happening in other tabs.
    this.speech_.cancel();
    this.speech_.speak(message);
  }

  private logSpeechPlaySession_() {
    const startTime = this.model_.getPlaySessionStartTime();
    if (startTime) {
      this.logger_.logSpeechPlaySession(
          startTime, this.voiceLanguageController_.getCurrentVoice());
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
