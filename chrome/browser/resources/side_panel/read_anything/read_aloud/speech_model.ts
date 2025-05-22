// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {WordBoundaryState} from './word_boundaries.js';

export enum PauseActionSource {
  DEFAULT,
  BUTTON_CLICK,
  VOICE_PREVIEW,
  VOICE_SETTINGS_CHANGE,
  ENGINE_INTERRUPT,
  SPEECH_FINISHED,
}

export interface SpeechPlayingState {
  // True when the user presses play, regardless of if audio has actually
  // started yet. This will be false when speech is paused.
  isSpeechActive: boolean;
  // When `isSpeechActive` is false, this indicates how it became false. e.g.
  // via pause button click or because other speech settings were changed.
  pauseSource: PauseActionSource;
  // Indicates that audio is currently playing.
  // When a user presses the play button, isSpeechActive becomes true, but
  // `isAudioCurrentlyPlaying` will tell us whether audio actually started
  // playing yet. This is a separate state because audio starting has a delay.
  isAudioCurrentlyPlaying: boolean;
  // Indicates if speech has been triggered on the current page by a play
  // button press. This will be true throughout the lifetime of reading
  // the content on the page. It will only be reset when speech has completely
  // stopped from reaching the end of content or changing pages. Pauses will
  // not update it.
  hasSpeechBeenTriggered: boolean;
  // If we're in the middle of repositioning speech, as this could cause a
  // this.speech_.cancel() that shouldn't update the UI for the speech playing
  // state.
  isSpeechBeingRepositioned: boolean;
}

export enum SpeechEngineState {
  NONE,
  LOADING,
  LOADED,
}

interface ReadingPosition {
  nodeId: number;
  offset: number;
}

export class SpeechModel {
  private speechPlayingState_: SpeechPlayingState = {
    isSpeechActive: false,
    pauseSource: PauseActionSource.DEFAULT,
    isAudioCurrentlyPlaying: false,
    hasSpeechBeenTriggered: false,
    isSpeechBeingRepositioned: false,
  };

  private speechEngineState_: SpeechEngineState = SpeechEngineState.NONE;
  private previewVoicePlaying_: SpeechSynthesisVoice|null = null;

  // With minor page changes, we redistill or redraw sometimes and end up losing
  // our reading position if read aloud has started. This keeps track of the
  // last position so we can check if it's still in the new page.
  private lastReadingPosition_: ReadingPosition|null = null;
  private savedSpeechPlayingState_: SpeechPlayingState|null = null;
  private savedWordBoundaryState_: WordBoundaryState|null = null;

  // Used for logging play time.
  private playSessionStartTime_: number|null = null;

  // If the node id of the first text node that should be used by Read Aloud
  // has been set. This is null if the id has not been set.
  private firstTextNodeSetForReadAloud_: number|null = null;

  private resumeSpeechOnVoiceMenuClose_: boolean = false;

  getSavedSpeechPlayingState(): SpeechPlayingState|null {
    return this.savedSpeechPlayingState_;
  }

  setSavedSpeechPlayingState(state: SpeechPlayingState|null): void {
    this.savedSpeechPlayingState_ = state;
  }

  getSavedWordBoundaryState(): WordBoundaryState|null {
    return this.savedWordBoundaryState_;
  }

  setSavedWordBoundaryState(state: WordBoundaryState|null): void {
    this.savedWordBoundaryState_ = state;
  }

  getResumeSpeechOnVoiceMenuClose(): boolean {
    return this.resumeSpeechOnVoiceMenuClose_;
  }

  setResumeSpeechOnVoiceMenuClose(shouldResume: boolean) {
    this.resumeSpeechOnVoiceMenuClose_ = shouldResume;
  }

  getFirstTextNode(): number|null {
    return this.firstTextNodeSetForReadAloud_;
  }

  setFirstTextNode(node: number|null) {
    this.firstTextNodeSetForReadAloud_ = node;
  }

  getPlaySessionStartTime(): number|null {
    return this.playSessionStartTime_;
  }

  setPlaySessionStartTime(time: number|null): void {
    this.playSessionStartTime_ = time;
  }

  getLastPosition(): ReadingPosition|null {
    return this.lastReadingPosition_;
  }

  setLastPosition(position: ReadingPosition|null) {
    this.lastReadingPosition_ = position;
  }

  getEngineState(): SpeechEngineState {
    return this.speechEngineState_;
  }

  setEngineState(state: SpeechEngineState): void {
    this.speechEngineState_ = state;
  }

  getPreviewVoicePlaying(): SpeechSynthesisVoice|null {
    return this.previewVoicePlaying_;
  }

  setPreviewVoicePlaying(voice: SpeechSynthesisVoice|null) {
    this.previewVoicePlaying_ = voice;
  }

  getState(): SpeechPlayingState {
    return this.speechPlayingState_;
  }

  setState(state: SpeechPlayingState): void {
    this.speechPlayingState_ = {...state};
  }

  isSpeechActive(): boolean {
    return this.speechPlayingState_.isSpeechActive;
  }

  setIsSpeechActive(value: boolean): void {
    this.speechPlayingState_.isSpeechActive = value;
  }

  getPauseSource(): PauseActionSource {
    return this.speechPlayingState_.pauseSource;
  }

  setPauseSource(value: PauseActionSource): void {
    this.speechPlayingState_.pauseSource = value;
  }

  isAudioCurrentlyPlaying(): boolean {
    return this.speechPlayingState_.isAudioCurrentlyPlaying;
  }

  setIsAudioCurrentlyPlaying(value: boolean): void {
    this.speechPlayingState_.isAudioCurrentlyPlaying = value;
  }

  hasSpeechBeenTriggered(): boolean {
    return this.speechPlayingState_.hasSpeechBeenTriggered;
  }

  setHasSpeechBeenTriggered(value: boolean): void {
    this.speechPlayingState_.hasSpeechBeenTriggered = value;
  }

  isSpeechBeingRepositioned(): boolean {
    return this.speechPlayingState_.isSpeechBeingRepositioned;
  }

  setIsSpeechBeingRepositioned(value: boolean): void {
    this.speechPlayingState_.isSpeechBeingRepositioned = value;
  }
}
