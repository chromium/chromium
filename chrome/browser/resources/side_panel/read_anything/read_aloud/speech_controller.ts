// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ReadAnythingLogger} from '../read_anything_logger.js';
import type {SpeechBrowserProxy} from '../speech_browser_proxy.js';
import {SpeechBrowserProxyImpl} from '../speech_browser_proxy.js';

import {PauseActionSource, SpeechModel} from './speech_model.js';
import type {SpeechPlayingState} from './speech_model.js';

export interface SpeechListener {
  onPause(): void;
  onIsSpeechActiveChange(): void;
  onIsAudioCurrentlyPlayingChange(): void;
}

export class SpeechController {
  private model_: SpeechModel = new SpeechModel();
  private speech_: SpeechBrowserProxy = SpeechBrowserProxyImpl.getInstance();
  private logger_: ReadAnythingLogger = ReadAnythingLogger.getInstance();
  private listeners_: SpeechListener[] = [];

  constructor() {
    // Send over the initial state.
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
      this.speech_.pause();
    } else {
      // Canceling clears all the Utterances that are queued up via synth.play()
      this.speech_.cancel();
    }

    this.listeners_.forEach(l => l.onPause());
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

  private isSpeechActiveChanged(isSpeechActive: boolean) {
    this.listeners_.forEach(l => l.onIsSpeechActiveChange());
    chrome.readingMode.onSpeechPlayingStateChanged(isSpeechActive);
  }

  static getInstance(): SpeechController {
    return instance || (instance = new SpeechController());
  }

  static setInstance(obj: SpeechController) {
    instance = obj;
  }
}

let instance: SpeechController|null = null;
