// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/polymer/v3_0/iron-selector/iron-selector.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_page_selector/cr_page_selector.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {loadTimeData} from './i18n_setup.js';
import type {PageHandlerRemote} from './new_tab_page.mojom-webui.js';
import {NewTabPageProxy} from './new_tab_page_proxy.js';
import {getCss} from './voice_search_overlay.css.js';
import {getHtml} from './voice_search_overlay.html.js';
import {WindowProxy} from './window_proxy.js';

/**
 * Threshold for considering an interim speech transcript result as "confident
 * enough". The more confident the API is about a transcript, the higher the
 * confidence (number between 0 and 1).
 */
const RECOGNITION_CONFIDENCE_THRESHOLD: number = 0.5;

/**
 * Maximum number of characters recognized before force-submitting a query.
 * Includes characters of non-confident recognition transcripts.
 */
const QUERY_LENGTH_LIMIT: number = 120;

/**
 * Time in milliseconds to wait before closing the UI if no interaction has
 * occurred.
 */
const IDLE_TIMEOUT_MS: number = 8000;

/**
 * Time in milliseconds to wait before closing the UI after an error has
 * occurred. This is a short timeout used when no click-target is present.
 */
const ERROR_TIMEOUT_SHORT_MS: number = 9000;

/**
 * Time in milliseconds to wait before closing the UI after an error has
 * occurred. This is a longer timeout used when there is a click-target is
 * present.
 */
const ERROR_TIMEOUT_LONG_MS: number = 24000;

// The minimum transition time for the volume rings.
const VOLUME_ANIMATION_DURATION_MIN_MS: number = 170;

// The range of the transition time for the volume rings.
const VOLUME_ANIMATION_DURATION_RANGE_MS: number = 10;

// The set of controller states.
enum State {
  // Initial state before voice recognition has been set up.
  UNINITIALIZED = -1,
  // Indicates that speech recognition has started, but no audio has yet
  // been captured.
  STARTED = 0,
  // Indicates that audio is being captured by the Web Speech API, but no
  // speech has yet been recognized. UI indicates that audio is being captured.
  AUDIO_RECEIVED = 1,
  // Indicates that speech has been recognized by the Web Speech API, but no
  // resulting transcripts have yet been received back. UI indicates that audio
  // is being captured and is pulsating audio button.
  SPEECH_RECEIVED = 2,
  // Indicates speech has been successfully recognized and text transcripts have
  // been reported back. UI indicates that audio is being captured and is
  // displaying transcripts received so far.
  RESULT_RECEIVED = 3,
  // Indicates that speech recognition has failed due to an error (or a no match
  // error) being received from the Web Speech API. A timeout may have occurred
  // as well. UI displays the error message.
  ERROR_RECEIVED = 4,
  // Indicates speech recognition has received a final search query but the UI
  // has not yet redirected. The UI is displaying the final query.
  RESULT_FINAL = 5,
}

/**
 * Action the user can perform while using voice search. This enum must match
 * the numbering for NewTabPageVoiceAction in enums.xml. These values are
 * persisted to logs. Entries should not be renumbered, removed or reused.
 */
export enum Action {
  ACTIVATE_SEARCH_BOX = 0,
  ACTIVATE_KEYBOARD = 1,
  CLOSE_OVERLAY = 2,
  QUERY_SUBMITTED = 3,
  SUPPORT_LINK_CLICKED = 4,
  TRY_AGAIN_LINK = 5,
  TRY_AGAIN_MIC_BUTTON = 6,  // Deprecated.
}

/**
 * Errors than can occur while using voice search. This enum must match the
 * numbering for NewTabPageVoiceError in enums.xml. These values are persisted
 * to logs. Entries should not be renumbered, removed or reused.
 */
export enum Error {
  ABORTED = 0,
  AUDIO_CAPTURE = 1,
  BAD_GRAMMAR = 2,
  LANGUAGE_NOT_SUPPORTED = 3,
  NETWORK = 4,
  NO_MATCH = 5,
  NO_SPEECH = 6,
  NOT_ALLOWED = 7,
  OTHER = 8,
  SERVICE_NOT_ALLOWED = 9,
}

export function recordVoiceAction(action: Action) {
  chrome.metricsPrivate.recordEnumerationValue(
      'NewTabPage.VoiceActions', action, Object.keys(Action).length);
}

/**
 * Returns the error type based on the error string received from the webkit
 * speech recognition API.
 * @param webkitError The error string received from the webkit speech
 *     recognition API.
 * @return The appropriate error state from the Error enum.
 */
function toError(webkitError: string): Error {
  switch (webkitError) {
    case 'aborted':
      return Error.ABORTED;
    case 'audio-capture':
      return Error.AUDIO_CAPTURE;
    case 'language-not-supported':
      return Error.LANGUAGE_NOT_SUPPORTED;
    case 'network':
      return Error.NETWORK;
    case 'no-speech':
      return Error.NO_SPEECH;
    case 'not-allowed':
      return Error.NOT_ALLOWED;
    case 'service-not-allowed':
      return Error.SERVICE_NOT_ALLOWED;
    case 'bad-grammar':
      return Error.BAD_GRAMMAR;
    default:
      return Error.OTHER;
  }
}

/**
 * Returns a timeout based on the error received from the webkit speech
 * recognition API.
 * @param error An error from the Error enum.
 * @return The appropriate timeout in MS for displaying the error.
 */
function getErrorTimeout(error: Error): number {
  switch (error) {
    case Error.AUDIO_CAPTURE:
    case Error.NO_SPEECH:
    case Error.NOT_ALLOWED:
    case Error.NO_MATCH:
      return ERROR_TIMEOUT_LONG_MS;
    default:
      return ERROR_TIMEOUT_SHORT_MS;
  }
}

// TODO(crbug.com/40449919): Remove when bug is fixed.
declare global {
  interface Window {
    webkitSpeechRecognition: typeof SpeechRecognition;
  }
}

export interface VoiceSearchOverlayElement {
  $: {
    dialog: HTMLDialogElement,
    micContainer: HTMLElement,
    micVolume: HTMLElement,
  };
}

// Overlay that lats the user perform voice searches.
export class VoiceSearchOverlayElement extends CrLitElement {
  static get is() {
    return 'ntp-voice-search-overlay';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      interimResult_: {type: String},
      finalResult_: {type: String},
      state_: {type: Number},
      error_: {type: Number},
      helpUrl_: {type: String},
      micVolumeLevel_: {type: Number},
      micVolumeDuration_: {type: Number},
    };
  }

  protected interimResult_: string;
  protected finalResult_: string;
  private state_: State = State.UNINITIALIZED;
  private error_: Error;
  protected helpUrl_: string =
      `https://support.google.com/chrome/?p=ui_voice_search&hl=${
          window.navigator.language}`;
  protected micVolumeLevel_: number = 0;
  protected micVolumeDuration_: number = VOLUME_ANIMATION_DURATION_MIN_MS;

  private pageHandler_: PageHandlerRemote;
  private voiceRecognition_: SpeechRecognition;
  private timerId_: number|null = null;

  constructor() {
    super();
    this.pageHandler_ = NewTabPageProxy.getInstance().handler;
    this.voiceRecognition_ = new window.webkitSpeechRecognition();
    this.voiceRecognition_.continuous = false;
    this.voiceRecognition_.interimResults = true;
    this.voiceRecognition_.lang = window.navigator.language;
    this.voiceRecognition_.onaudiostart = this.onAudioStart_.bind(this);
    this.voiceRecognition_.onspeechstart = this.onSpeechStart_.bind(this);
    this.voiceRecognition_.onresult = this.onResult_.bind(this);
    this.voiceRecognition_.onend = this.onEnd_.bind(this);
    this.voiceRecognition_.onerror = (e) => {
      this.onError_(toError(e.error));
    };
    this.voiceRecognition_.onnomatch = () => {
      this.onError_(Error.NO_MATCH);
    };
  }

  override connectedCallback() {
    super.connectedCallback();
    this.$.dialog.showModal();
    this.start();
  }

  private start() {
    this.voiceRecognition_.start();
    this.state_ = State.STARTED;
    this.resetIdleTimer_();
  }

  protected onOverlayClose_() {
    this.voiceRecognition_.abort();
    this.dispatchEvent(new Event('close'));
  }

  protected onOverlayClick_() {
    this.$.dialog.close();
    recordVoiceAction(Action.CLOSE_OVERLAY);
  }

  /**
   * Handles <ENTER> or <SPACE> to trigger a query if we have recognized speech.
   */
  protected onOverlayKeydown_(e: KeyboardEvent) {
    if (['Enter', ' '].includes(e.key) && this.finalResult_) {
      this.onFinalResult_();
    } else if (e.key === 'Escape') {
      this.onOverlayClick_();
    }
  }

  /**
   * Handles <ENTER> or <SPACE> to simulate click.
   */
  protected onLinkKeydown_(e: KeyboardEvent) {
    if (!['Enter', ' '].includes(e.key)) {
      return;
    }
    // Otherwise, we may trigger overlay-wide keyboard shortcuts.
    e.stopPropagation();
    // Otherwise, we open the link twice.
    e.preventDefault();
    (e.target as HTMLElement).click();
  }

  protected onLearnMoreClick_() {
    recordVoiceAction(Action.SUPPORT_LINK_CLICKED);
  }

  protected onTryAgainClick_(e: Event) {
    // Otherwise, we close the overlay.
    e.stopPropagation();
    this.start();
    recordVoiceAction(Action.TRY_AGAIN_LINK);
  }

  private resetIdleTimer_() {
    WindowProxy.getInstance().clearTimeout(this.timerId_);
    this.timerId_ = WindowProxy.getInstance().setTimeout(
        this.onIdleTimeout_.bind(this), IDLE_TIMEOUT_MS);
  }

  private onIdleTimeout_() {
    if (this.state_ === State.RESULT_FINAL) {
      // Waiting for query redirect.
      return;
    }
    if (this.finalResult_) {
      // Query what we recognized so far.
      this.onFinalResult_();
      return;
    }
    this.voiceRecognition_.abort();
    this.onError_(Error.NO_MATCH);
  }

  private resetErrorTimer_(duration: number) {
    WindowProxy.getInstance().clearTimeout(this.timerId_);
    this.timerId_ = WindowProxy.getInstance().setTimeout(() => {
      this.$.dialog.close();
    }, duration);
  }

  private onAudioStart_() {
    this.resetIdleTimer_();
    this.state_ = State.AUDIO_RECEIVED;
  }

  private onSpeechStart_() {
    this.resetIdleTimer_();
    this.state_ = State.SPEECH_RECEIVED;
    this.animateVolume_();
  }

  private onResult_(e: SpeechRecognitionEvent) {
    this.resetIdleTimer_();

    switch (this.state_) {
      case State.STARTED:
        // Network bugginess (the onspeechstart packet was lost).
        this.onAudioStart_();
        this.onSpeechStart_();
        break;
      case State.AUDIO_RECEIVED:
        // Network bugginess (the onaudiostart packet was lost).
        this.onSpeechStart_();
        break;
      case State.SPEECH_RECEIVED:
      case State.RESULT_RECEIVED:
        // Normal, expected states for processing results.
        break;
      default:
        // Not expecting results in any other states.
        return;
    }

    const results = e.results;
    if (results.length === 0) {
      return;
    }
    this.state_ = State.RESULT_RECEIVED;
    this.interimResult_ = '';
    this.finalResult_ = '';

    const finalResult = results[e.resultIndex];
    // Process final results.
    if (finalResult.isFinal) {
      this.finalResult_ = finalResult[0].transcript;
      this.onFinalResult_();
      return;
    }

    // Process interim results.
    for (let j = 0; j < results.length; j++) {
      const result = results[j][0];
      if (result.confidence > RECOGNITION_CONFIDENCE_THRESHOLD) {
        this.finalResult_ += result.transcript;
      } else {
        this.interimResult_ += result.transcript;
      }
    }

    // Force-stop long queries.
    if (this.interimResult_.length > QUERY_LENGTH_LIMIT) {
      this.onFinalResult_();
    }
  }

  private onFinalResult_() {
    if (!this.finalResult_) {
      this.onError_(Error.NO_MATCH);
      return;
    }
    this.state_ = State.RESULT_FINAL;
    const searchParams = new URLSearchParams();
    searchParams.append('q', this.finalResult_);
    // Add a parameter to indicate that this request is a voice search.
    searchParams.append('gs_ivs', '1');
    // Build the query URL.
    const queryUrl =
        new URL('/search', loadTimeData.getString('googleBaseUrl'));
    queryUrl.search = searchParams.toString();
    recordVoiceAction(Action.QUERY_SUBMITTED);
    WindowProxy.getInstance().navigate(queryUrl.href);
  }

  private onEnd_() {
    switch (this.state_) {
      case State.STARTED:
        this.onError_(Error.AUDIO_CAPTURE);
        return;
      case State.AUDIO_RECEIVED:
        this.onError_(Error.NO_SPEECH);
        return;
      case State.SPEECH_RECEIVED:
      case State.RESULT_RECEIVED:
        this.onError_(Error.NO_MATCH);
        return;
      case State.ERROR_RECEIVED:
      case State.RESULT_FINAL:
        return;
      default:
        this.onError_(Error.OTHER);
        return;
    }
  }

  private onError_(error: Error) {
    chrome.metricsPrivate.recordEnumerationValue(
        'NewTabPage.VoiceErrors', error, Object.keys(Error).length);
    if (error === Error.ABORTED) {
      // We are in the process of closing voice search.
      return;
    }
    this.error_ = error;
    this.state_ = State.ERROR_RECEIVED;
    this.resetErrorTimer_(getErrorTimeout(error));
  }

  private animateVolume_() {
    this.micVolumeLevel_ = 0;
    this.micVolumeDuration_ = VOLUME_ANIMATION_DURATION_MIN_MS;
    if (this.state_ !== State.SPEECH_RECEIVED &&
        this.state_ !== State.RESULT_RECEIVED) {
      return;
    }
    this.micVolumeLevel_ = WindowProxy.getInstance().random();
    this.micVolumeDuration_ = Math.round(
        VOLUME_ANIMATION_DURATION_MIN_MS +
        WindowProxy.getInstance().random() *
            VOLUME_ANIMATION_DURATION_RANGE_MS);
    WindowProxy.getInstance().setTimeout(
        this.animateVolume_.bind(this), this.micVolumeDuration_);
  }

  protected getText_(): string {
    switch (this.state_) {
      case State.STARTED:
        return 'waiting';
      case State.AUDIO_RECEIVED:
      case State.SPEECH_RECEIVED:
        return 'speak';
      case State.RESULT_RECEIVED:
      case State.RESULT_FINAL:
        return 'result';
      case State.ERROR_RECEIVED:
        return 'error';
      default:
        return 'none';
    }
  }

  protected getErrorText_(): string {
    switch (this.error_) {
      case Error.NO_SPEECH:
        return 'no-speech';
      case Error.AUDIO_CAPTURE:
        return 'audio-capture';
      case Error.NETWORK:
        return 'network';
      case Error.NOT_ALLOWED:
      case Error.SERVICE_NOT_ALLOWED:
        return 'not-allowed';
      case Error.LANGUAGE_NOT_SUPPORTED:
        return 'language-not-supported';
      case Error.NO_MATCH:
        return 'no-match';
      case Error.ABORTED:
      case Error.OTHER:
      default:
        return 'other';
    }
  }

  protected getErrorLink_(): string {
    switch (this.error_) {
      case Error.NO_SPEECH:
      case Error.AUDIO_CAPTURE:
        return 'learn-more';
      case Error.NOT_ALLOWED:
      case Error.SERVICE_NOT_ALLOWED:
        return 'details';
      case Error.NO_MATCH:
        return 'try-again';
      default:
        return 'none';
    }
  }

  protected getMicClass_(): string {
    switch (this.state_) {
      case State.AUDIO_RECEIVED:
        return 'listening';
      case State.SPEECH_RECEIVED:
      case State.RESULT_RECEIVED:
        return 'receiving';
      default:
        return '';
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ntp-voice-search-overlay': VoiceSearchOverlayElement;
  }
}

customElements.define(VoiceSearchOverlayElement.is, VoiceSearchOverlayElement);
