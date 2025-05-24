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
  private listeners_: SpeechListener[] = [];

  constructor() {
    // Send over the initial state.
    this.clearReadAloudState();
    this.isSpeechActiveChanged(this.isSpeechActive());
  }

  addListener(listener: SpeechListener) {
    this.listeners_.push(listener);
  }

  private setState_(state: SpeechPlayingState) {
    const wasSpeechActive = this.isSpeechActive();
    const wasAudioPlaying = this.isAudioCurrentlyPlaying();
    this.model_.setState(state);
    if (state.isSpeechActive !== wasSpeechActive) {
      this.isSpeechActiveChanged(state.isSpeechActive);
    }
    if (state.isAudioCurrentlyPlaying !== wasAudioPlaying) {
      this.listeners_.forEach(l => l.onIsAudioCurrentlyPlayingChange());
    }
  }

  isSpeechActive(): boolean {
    return this.model_.isSpeechActive();
  }

  private setIsSpeechActive_(isSpeechActive: boolean) {
    if (isSpeechActive !== this.isSpeechActive()) {
      this.model_.setIsSpeechActive(isSpeechActive);
      this.isSpeechActiveChanged(isSpeechActive);
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
      this.listeners_.forEach(l => l.onIsAudioCurrentlyPlayingChange());
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
    return chrome.readingMode.isSpeechTreeInitialized;
  }

  isPausedFromButton(): boolean {
    return this.model_.getPauseSource() === PauseActionSource.BUTTON_CLICK;
  }

  isTemporaryPause(): boolean {
    const source = this.model_.getPauseSource();
    return (source === PauseActionSource.VOICE_PREVIEW) ||
        (source === PauseActionSource.VOICE_SETTINGS_CHANGE);
  }

  getSelectionAdjustedForHighlights(
      anchorNode: Node, anchorOffset: number, focusNode: Node,
      focusOffset: number): {
    anchorNodeId: number|undefined,
    anchorOffset: number,
    focusNodeId: number|undefined,
    focusOffset: number,
  } {
    let anchorNodeId = this.nodeStore_.getAxId(anchorNode);
    let focusNodeId = this.nodeStore_.getAxId(focusNode);
    let adjustedAnchorOffset = anchorOffset;
    let adjustedFocusOffset = focusOffset;
    if (!anchorNodeId) {
      anchorNodeId = this.highlighter_.getAncestorId(anchorNode);
      adjustedAnchorOffset += this.highlighter_.getOffsetInAncestor(anchorNode);
    }
    if (!focusNodeId) {
      focusNodeId = this.highlighter_.getAncestorId(focusNode);
      adjustedFocusOffset += this.highlighter_.getOffsetInAncestor(focusNode);
    }
    return {
      anchorNodeId: anchorNodeId,
      anchorOffset: adjustedAnchorOffset,
      focusNodeId: focusNodeId,
      focusOffset: adjustedFocusOffset,
    };
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
  }

  onSelectionChange() {
    // If speech is resumed, this won't be restored.
    // TODO: crbug.com/40927698 - Restore the previous highlight after
    // speech is resumed after a selection.
    this.highlighter_.clearHighlightFormatting();
  }

  // If the screen is locked during speech, we should stop speaking.
  onLockScreen() {
    if (this.isSpeechActive()) {
      this.stopSpeech_(PauseActionSource.DEFAULT);
    }
  }

  onVoiceSelected(selectedVoice: SpeechSynthesisVoice) {
    const currentVoice = this.voiceLanguageController_.getCurrentVoice();
    this.voiceLanguageController_.setUserPreferredVoice(selectedVoice);

    // If the locales are identical, the voices are likely from the same
    // TTS engine, therefore, we don't need to reset the word boundary state.
    if (currentVoice?.lang.toLowerCase() !== selectedVoice.lang.toLowerCase()) {
      this.wordBoundaries_.resetToDefaultState(
          /*possibleWordBoundarySupportChange=*/ true);
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
      this.resumeSpeech_(null);
    }
  }

  onHighlightGranularityChange(newGranularity: number) {
    chrome.readingMode.onHighlightGranularityChanged(newGranularity);

    // Rehighlight the new granularity.
    if (newGranularity !== chrome.readingMode.noHighlighting) {
      this.highlightCurrentGranularity_(chrome.readingMode.getCurrentText());
    }

    this.logger_.logHighlightGranularity(newGranularity);
  }

  onLinksToggled() {
    // Rehighlight the current granularity text after links have been
    // toggled on or off to ensure the entire granularity segment is
    // highlighted.
    if (this.highlighter_.hasCurrentHighlights()) {
      this.highlightCurrentGranularity_(chrome.readingMode.getCurrentText());
    }
  }

  onPlayPauseToggle(selection: Selection|null, textContent: string|null) {
    if (this.isSpeechActive()) {
      this.stopSpeech_(PauseActionSource.BUTTON_CLICK);
    } else {
      this.playSpeech_(selection, textContent);
      this.model_.setPlaySessionStartTime(Date.now());
    }
  }

  private playSpeech_(selection: Selection|null, textContent: string|null) {
    if (this.hasSpeechBeenTriggered() && !this.isSpeechActive()) {
      this.resumeSpeech_(selection);
    } else {
      this.playSpeechForTheFirstTime_(selection, textContent);
    }
  }

  onNextGranularityClick() {
    this.moveGranularity_();
    chrome.readingMode.movePositionToNextGranularity();

    if (!this.highlightAndPlayMessage_()) {
      this.onSpeechFinished_();
    }
  }

  onPreviousGranularityClick() {
    // This must be called BEFORE calling
    // chrome.readingMode.movePositionToPreviousGranularity so we can accurately
    // determine what's currently being highlighted.
    this.highlighter_.removeCurrentHighlight();
    this.moveGranularity_();
    chrome.readingMode.movePositionToPreviousGranularity();

    if (!this.highlightAndPlayMessage_(
            /*isInterrupted=*/ false,
            /*isMovingBackward=*/ true)) {
      this.onSpeechFinished_();
    }
  }

  private moveGranularity_() {
    this.model_.setIsSpeechBeingRepositioned(true);
    this.highlighter_.resetPreviousHighlight();

    // Reset the word boundary index whenever we move the granularity position.
    this.wordBoundaries_.resetToDefaultState();
  }

  private resumeSpeech_(selection: Selection|null) {
    let playedFromSelection = false;
    if (this.hasSelection_(selection)) {
      this.wordBoundaries_.resetToDefaultState();
      playedFromSelection = this.playFromSelection_(selection);
    }

    if (!playedFromSelection) {
      if (this.isPausedFromButton() && !this.wordBoundaries_.hasBoundaries()) {
        // If word boundaries aren't supported for the given voice, we should
        // still continue to use synth.resume, as this is preferable to
        // restarting the current message.
        this.speech_.resume();
      } else {
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
      this.highlightCurrentGranularity_(chrome.readingMode.getCurrentText());
    }
  }

  private playSpeechForTheFirstTime_(
      selection: Selection|null, textContent: string|null) {
    if (!textContent) {
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

    const playedFromSelection = this.playFromSelection_(selection);
    if (playedFromSelection) {
      return;
    }

    this.initializeSpeechTree();
    if (this.isSpeechTreeInitialized() && !this.highlightAndPlayMessage_()) {
      // Ensure we're updating Read Aloud state if there's no text to speak.
      this.onSpeechFinished_();
    }
  }

  private hasSelection_(selection: Selection|null): boolean {
    return (selection !== null) &&
        (selection.anchorNode !== selection.focusNode ||
         selection.anchorOffset !== selection.focusOffset);
  }

  private playFromSelection_(selection: Selection|null): boolean {
    if (!this.isSpeechTreeInitialized() || !selection ||
        !this.hasSelection_(selection)) {
      return false;
    }

    const anchorNodeId = chrome.readingMode.startNodeId;
    const anchorOffset = chrome.readingMode.startOffset;
    const focusNodeId = chrome.readingMode.endNodeId;
    const focusOffset = chrome.readingMode.endOffset;

    // If only one of the ids is present, use that one.
    let startingNodeId: number|undefined =
        anchorNodeId ? anchorNodeId : focusNodeId;
    let startingOffset = anchorNodeId ? anchorOffset : focusOffset;
    // If both are present, start with the node that is sooner in the page.
    if (anchorNodeId && focusNodeId) {
      if (anchorNodeId === focusNodeId) {
        startingOffset = Math.min(anchorOffset, focusOffset);
      } else if (selection.anchorNode && selection.focusNode) {
        const pos =
            selection.anchorNode.compareDocumentPosition(selection.focusNode);
        const focusIsFirst = pos === Node.DOCUMENT_POSITION_PRECEDING;
        startingNodeId = focusIsFirst ? focusNodeId : anchorNodeId;
        startingOffset = focusIsFirst ? focusOffset : anchorOffset;
      }
    }

    if (!startingNodeId) {
      return false;
    }

    // Clear the selection so we don't keep trying to play from the same
    // selection every time they press play.
    selection.removeAllRanges();
    // Iterate through the page from the beginning until we get to the
    // selection. This is so clicking previous works before the selection and
    // so the previous highlights are properly set.
    chrome.readingMode.resetGranularityIndex();
    // Iterate through the nodes asynchronously so that we can show the spinner
    // in the toolbar while we move up to the selection.
    setTimeout(() => {
      this.movePlaybackToNode_(startingNodeId, startingOffset);
      // Set everything to previous and then play the next granularity, which
      // includes the selection.
      this.highlighter_.resetPreviousHighlight();
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

    this.highlightCurrentGranularity_(axNodeIds);
    return true;
  }

  private skipCurrentPosition_(
      isInterrupted: boolean, isMovingBackward: boolean): boolean {
    if (isMovingBackward) {
      chrome.readingMode.movePositionToPreviousGranularity();
    } else {
      chrome.readingMode.movePositionToNextGranularity();
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
    const endBoundary =
        this.getUtteranceEndBoundary_(utteranceText, isTextTooLong);
    this.playTextWithBoundaries_(utteranceText, isTextTooLong, endBoundary);
  }

  private playTextWithBoundaries_(
      utteranceText: string, isTextTooLong: boolean, endBoundary: number) {
    const message =
        new SpeechSynthesisUtterance(utteranceText.substring(0, endBoundary));

    message.onerror = (error) => {
      this.handleSpeechSynthesisError_(error, utteranceText);
    };

    this.setOnBoundary_(message);
    this.setOnSpeechSynthesisUtteranceStart_(message);

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
      if (!this.highlightAndPlayMessage_()) {
        this.onSpeechFinished_();
      }
    };

    this.speakMessage_(message);
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
          utteranceText, true,
          this.getUtteranceEndBoundary_(utteranceText, true));
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

  private stopSpeech_(pauseSource: PauseActionSource) {
    // Pause source needs to be set before updating isSpeechActive so that
    // listeners get the correct source when listening for isSpeechActive
    // changes.
    this.model_.setPauseSource(pauseSource);
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
    if (this.isPausedFromButton()) {
      this.logSpeechPlaySession_();
      this.speech_.pause();
    } else {
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
      this.resumeSpeech_(null);
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
    this.highlighter_.clearHighlightFormatting();
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
    this.model_.setFirstTextNode(null);
    this.model_.setResumeSpeechOnVoiceMenuClose(false);
  }

  saveReadAloudState() {
    this.model_.setSavedSpeechPlayingState({...this.model_.getState()});
    this.model_.setSavedWordBoundaryState({...this.wordBoundaries_.state});
  }

  setPreviousReadingPositionIfExists() {
    const savedSpeechPlayingState = this.model_.getSavedSpeechPlayingState();
    const savedWordBoundaryState = this.model_.getSavedWordBoundaryState();
    const lastPosition = this.model_.getLastPosition();
    this.model_.setSavedSpeechPlayingState(null);
    this.model_.setSavedWordBoundaryState(null);
    if (!savedWordBoundaryState || !savedSpeechPlayingState ||
        !savedSpeechPlayingState.hasSpeechBeenTriggered || !lastPosition) {
      return;
    }

    if (this.nodeStore_.getDomNode(lastPosition.nodeId)) {
      this.movePlaybackToNode_(lastPosition.nodeId, lastPosition.offset);
      this.setState_(savedSpeechPlayingState);
      this.wordBoundaries_.state = savedWordBoundaryState;
      // Since we're setting the reading position after a content update when
      // we're paused, redraw the highlight after moving the traversal state to
      // the right spot above.
      this.highlightCurrentGranularity_(chrome.readingMode.getCurrentText());
    } else {
      this.model_.setLastPosition(null);
    }
  }

  private movePlaybackToNode_(nodeId: number, offset: number): void {
    let currentTextIds = chrome.readingMode.getCurrentText();
    let hasCurrentText = currentTextIds.length > 0;
    // Since a node could spread across multiple granularities, we use the
    // offset to determine if the selected text is in this granularity or if
    // we have to move to the next one.
    let startOfSelectionIsInCurrentText = currentTextIds.includes(nodeId) &&
        chrome.readingMode.getCurrentTextEndIndex(nodeId) > offset;
    while (hasCurrentText && !startOfSelectionIsInCurrentText) {
      this.highlightCurrentGranularity_(
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
  private highlightCurrentGranularity_(
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

  private isTextTooLong_(text: string): boolean {
    return !this.voiceLanguageController_.getCurrentVoice()?.localService &&
        text.length > MAX_SPEECH_LENGTH;
  }

  private getUtteranceEndBoundary_(text: string, isTextTooLong: boolean):
      number {
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
