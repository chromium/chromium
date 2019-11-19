// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Get the preferred language for UI localization. Represents Chrome's UI
 * language, which might not coincide with the user's "preferred" language
 * in the Settings. For more details, see:
 *  - https://developer.mozilla.org/en/docs/Web/API/NavigatorLanguage/language
 *  - https://developer.mozilla.org/en/docs/Web/API/NavigatorLanguage/languages
 *
 * The returned value is a language version string as defined in
 * <a href="http://www.ietf.org/rfc/bcp/bcp47.txt">BCP 47</a>.
 * Examples: "en", "en-US", "cs-CZ", etc.
 */
function getChromeUILanguage() {
  // In Chrome, |window.navigator.language| is not guaranteed to be equal to
  // |window.navigator.languages[0]|.
  return window.navigator.language;
}

/**
 * The different types of user action and error events that are logged
 * from Voice Search. This enum is used to transfer information to
 * the renderer and is not used as a UMA enum histogram's logged value.
 * Note: Keep in sync with common/ntp_logging_events.h
 * @enum {!number}
 * @const
 */
const LOG_TYPE = {
  // Activated by clicking on the fakebox icon.
  ACTION_ACTIVATE_FAKEBOX: 13,
  // Activated by keyboard shortcut.
  ACTION_ACTIVATE_KEYBOARD: 14,
  // Close the voice overlay by a user's explicit action.
  ACTION_CLOSE_OVERLAY: 15,
  // Submitted voice query.
  ACTION_QUERY_SUBMITTED: 16,
  // Clicked on support link in error message.
  ACTION_SUPPORT_LINK_CLICKED: 17,
  // Retried by clicking Try Again link.
  ACTION_TRY_AGAIN_LINK: 18,
  // Retried by clicking microphone button.
  ACTION_TRY_AGAIN_MIC_BUTTON: 10,
  // Errors received from the Speech Recognition API.
  ERROR_NO_SPEECH: 20,
  ERROR_ABORTED: 21,
  ERROR_AUDIO_CAPTURE: 22,
  ERROR_NETWORK: 23,
  ERROR_NOT_ALLOWED: 24,
  ERROR_SERVICE_NOT_ALLOWED: 25,
  ERROR_BAD_GRAMMAR: 26,
  ERROR_LANGUAGE_NOT_SUPPORTED: 27,
  ERROR_NO_MATCH: 28,
  ERROR_OTHER: 29
};

/**
 * Enum for keyboard event codes.
 * @enum {!string}
 * @const
 */
const KEYCODE = {
  ENTER: 'Enter',
  ESC: 'Escape',
  NUMPAD_ENTER: 'NumpadEnter',
  PERIOD: 'Period',
  SPACE: 'Space',
  TAB: 'Tab'
};

/**
 * The set of possible recognition errors.
 * @enum {!number}
 * @const
 */
const RecognitionError = {
  NO_SPEECH: 0,
  ABORTED: 1,
  AUDIO_CAPTURE: 2,
  NETWORK: 3,
  NOT_ALLOWED: 4,
  SERVICE_NOT_ALLOWED: 5,
  BAD_GRAMMAR: 6,
  LANGUAGE_NOT_SUPPORTED: 7,
  NO_MATCH: 8,
  OTHER: 9
};

/**
 * Provides methods for communicating with the <a
 * href="https://developer.mozilla.org/en-US/docs/Web/API/Web_Speech_API">
 * Web Speech API</a>, error handling and executing search queries.
 */
const speech = {};

/**
 * Localized translations for messages used in the Speech UI.
 * @type {{
 *   audioError: string,
 *   details: string,
 *   languageError: string,
 *   learnMore: string,
 *   listening: string,
 *   networkError: string,
 *   noTranslation: string,
 *   noVoice: string,
 *   otherError: string,
 *   permissionError: string,
 *   ready: string,
 *   tryAgain: string,
 *   waiting: string
 * }}
 */
speech.messages = {
  audioError: '',
  details: '',
  languageError: '',
  learnMore: '',
  listening: '',
  networkError: '',
  noTranslation: '',
  noVoice: '',
  otherError: '',
  permissionError: '',
  ready: '',
  tryAgain: '',
  waiting: ''
};

/**
 * The set of controller states.
 * @enum {number}
 * @private
 */
speech.State_ = {
  // Initial state of the controller. It is never re-entered.
  // The only state from which the |speech.init()| method can be called.
  // The UI overlay is hidden, recognition is inactive.
  UNINITIALIZED: -1,
  // Represents a ready to be activated state. If voice search is unsuccessful
  // for any reason, the controller will return to this state
  // using |speech.reset_()|. The UI overlay is hidden, recognition is inactive.
  READY: 0,
  // Indicates that speech recognition has started, but no audio has yet
  // been captured. The UI overlay is visible, recognition is active.
  STARTED: 1,
  // Indicates that audio is being captured by the Web Speech API, but no
  // speech has yet been recognized. The UI overlay is visible and indicating
  // that audio is being captured, recognition is active.
  AUDIO_RECEIVED: 2,
  // Represents a state where speech has been recognized by the Web Speech API,
  // but no resulting transcripts have yet been received back. The UI overlay is
  // visible and indicating that audio is being captured, recognition is active.
  SPEECH_RECEIVED: 3,
  // Controller state where speech has been successfully recognized and text
  // transcripts have been reported back. The UI overlay is visible
  // and displaying intermediate results, recognition is active.
  // This state remains until recognition ends successfully or due to an error.
  RESULT_RECEIVED: 4,
  // Indicates that speech recognition has failed due to an error
  // (or a no match error) being received from the Web Speech API.
  // A timeout may have occurred as well. The UI overlay is visible
  // and displaying an error message, recognition is inactive.
  ERROR_RECEIVED: 5,
  // Represents a state where speech recognition has been stopped
  // (either on success or failure) and the UI has not yet reset/redirected.
  // The UI overlay is displaying results or an error message with a timeout,
  // after which the site will either get redirected to search results
  // (successful) or back to the NTP by hiding the overlay (unsuccessful).
  STOPPED: 6
};

/**
 * Threshold for considering an interim speech transcript result as "confident
 * enough". The more confident the API is about a transcript, the higher the
 * confidence (number between 0 and 1).
 * @private {number}
 * @const
 */
speech.RECOGNITION_CONFIDENCE_THRESHOLD_ = 0.5;

/**
 * Time in milliseconds to wait before closing the UI after an error has
 * occured. This is a short timeout used when no click-target is present.
 * @private {number}
 * @const
 */
speech.ERROR_TIMEOUT_SHORT_MS_ = 3000;

/**
 * Time in milliseconds to wait before closing the UI after an error has
 * occured. This is a longer timeout used when there is a click-target is
 * present.
 * @private {number}
 * @const
 */
speech.ERROR_TIMEOUT_LONG_MS_ = 8000;

/**
 * Time in milliseconds to wait before closing the UI if no interaction has
 * occured.
 * @private {number}
 * @const
 */
speech.IDLE_TIMEOUT_MS_ = 8000;

/**
 * Maximum number of characters recognized before force-submitting a query.
 * Includes characters of non-confident recognition transcripts.
 * @private {number}
 * @const
 */
speech.QUERY_LENGTH_LIMIT_ = 120;

/**
 * Specifies the current state of the controller.
 * Note: Different than the UI state.
 * @private {speech.State_}
 */
speech.currentState_ = speech.State_.UNINITIALIZED;

/**
 * The ID for the error timer.
 * @private {number}
 */
speech.errorTimer_;

/**
 * The duration of the timeout for the UI elements during an error state.
 * Depending on the error state, we have different durations for the timeout.
 * @private {number}
 */
speech.errorTimeoutMs_ = 0;

/**
 * The last high confidence voice transcript received from the Web Speech API.
 * This is the actual query that could potentially be submitted to Search.
 * @private {string}
 */
speech.finalResult_;

/**
 * Base URL for sending queries to Search. Includes trailing forward slash.
 * @private {string}
 */
speech.googleBaseUrl_;

/**
 * The ID for the idle timer.
 * @private {number}
 */
speech.idleTimer_;

/**
 * The last low confidence voice transcript received from the Web Speech API.
 * @private {string}
 */
speech.interimResult_;

/**
 * The Web Speech API object driving the speech recognition transaction.
 * @private {!webkitSpeechRecognition}
 */
speech.recognition_;

/**
 * Indicates if the user is using keyboard navigation (i.e. tab).
 * @private {boolean}
 */
speech.usingKeyboardNavigation_ = false;

/**
 * Log an event from Voice Search.
 * @param {number} eventType Event from |LOG_TYPE|.
 */
speech.logEvent = function(eventType) {
  window.chrome.embeddedSearch.newTabPage.logEvent(eventType);
};

/**
 * Initialize the speech module as part of the local NTP. Adds event handlers
 * and shows the fakebox microphone icon.
 * @param {string} googleBaseUrl Base URL for sending queries to Search.
 * @param {!Object} translatedStrings Dictionary of localized string messages.
 * @param {?Element} fakeboxMicrophoneElem Fakebox microphone icon element.
 * @param {!Object} searchboxApiHandle SearchBox API handle.
 */
speech.init = function(
    googleBaseUrl, translatedStrings, fakeboxMicrophoneElem,
    searchboxApiHandle) {
  if (!fakeboxMicrophoneElem) {
    throw new Error('Speech button element not found.');
  }

  if (speech.currentState_ != speech.State_.UNINITIALIZED) {
    throw new Error(
        'Trying to re-initialize speech when not in UNINITIALIZED state.');
  }

  // Initialize event handlers.
  fakeboxMicrophoneElem.hidden = false;
  fakeboxMicrophoneElem.title = translatedStrings.fakeboxMicrophoneTooltip;
  fakeboxMicrophoneElem.onclick = function(event) {
    // If propagated, closes the overlay (click on the background).
    event.stopPropagation();
    speech.logEvent(LOG_TYPE.ACTION_ACTIVATE_FAKEBOX);
    speech.start();
  };
  fakeboxMicrophoneElem.onkeydown = function(event) {
    if (!event.repeat && speech.isSpaceOrEnter_(event.code) &&
        speech.currentState_ == speech.State_.READY) {
      event.stopPropagation();
      speech.start();
    }
  };
  window.addEventListener('keydown', speech.onKeyDown);
  if (searchboxApiHandle.onfocuschange) {
    throw new Error('OnFocusChange handler already set on searchbox.');
  }
  searchboxApiHandle.onfocuschange = speech.onOmniboxFocused;

  // Initialize speech internal state.
  speech.googleBaseUrl_ = googleBaseUrl;
  speech.messages = {
    audioError: translatedStrings.audioError,
    details: translatedStrings.details,
    languageError: translatedStrings.languageError,
    learnMore: translatedStrings.learnMore,
    listening: translatedStrings.listening,
    networkError: translatedStrings.networkError,
    noTranslation: translatedStrings.noTranslation,
    noVoice: translatedStrings.noVoice,
    otherError: translatedStrings.otherError,
    permissionError: translatedStrings.permissionError,
    ready: translatedStrings.ready,
    tryAgain: translatedStrings.tryAgain,
    waiting: translatedStrings.waiting,
  };
  view.init(speech.onClick_);
  view.setTitles(translatedStrings);
  speech.initWebkitSpeech_();
  speech.reset_();
};

/**
 * Initializes and configures the speech recognition API.
 * @private
 */
speech.initWebkitSpeech_ = function() {
  speech.recognition_ = new webkitSpeechRecognition();
  speech.recognition_.continuous = false;
  speech.recognition_.interimResults = true;
  speech.recognition_.lang = getChromeUILanguage();
  speech.recognition_.onaudiostart = speech.handleRecognitionAudioStart_;
  speech.recognition_.onend = speech.handleRecognitionEnd_;
  speech.recognition_.onerror = speech.handleRecognitionError_;
  speech.recognition_.onnomatch = speech.handleRecognitionOnNoMatch_;
  speech.recognition_.onresult = speech.handleRecognitionResult_;
  speech.recognition_.onspeechstart = speech.handleRecognitionSpeechStart_;
};

/**
 * Sets up the necessary states for voice search and then starts the
 * speech recognition interface.
 */
speech.start = function() {
  view.show();

  speech.resetIdleTimer_(speech.IDLE_TIMEOUT_MS_);

  document.addEventListener(
      'webkitvisibilitychange', speech.onVisibilityChange_, false);

  // Initialize |speech.recognition_| if it isn't already.
  if (!speech.recognition_) {
    speech.initWebkitSpeech_();
  }

  // If |speech.start()| is called too soon after |speech.stop()| then the
  // recognition interface hasn't yet reset and an error occurs. In this case
  // we need to hard-reset it and reissue the |recognition_.start()| command.
  try {
    speech.recognition_.start();
    speech.currentState_ = speech.State_.STARTED;
  } catch (error) {
    speech.initWebkitSpeech_();
    try {
      speech.recognition_.start();
      speech.currentState_ = speech.State_.STARTED;
    } catch (error2) {
      speech.stop();
    }
  }
};

/**
 * Hides the overlay and resets the speech state.
 */
speech.stop = function() {
  speech.recognition_.abort();
  speech.currentState_ = speech.State_.STOPPED;
  view.hide();
  speech.reset_();
};

/**
 * Resets the internal state to the READY state.
 * @private
 */
speech.reset_ = function() {
  window.clearTimeout(speech.idleTimer_);
  window.clearTimeout(speech.errorTimer_);

  document.removeEventListener(
      'webkitvisibilitychange', speech.onVisibilityChange_, false);

  speech.interimResult_ = '';
  speech.finalResult_ = '';
  speech.currentState_ = speech.State_.READY;
  speech.usingKeyboardNavigation_ = false;
};

/**
 * Informs the view that the browser is receiving audio input.
 * @param {Event=} opt_event Emitted event for audio start.
 * @private
 */
speech.handleRecognitionAudioStart_ = function(opt_event) {
  speech.resetIdleTimer_(speech.IDLE_TIMEOUT_MS_);
  speech.currentState_ = speech.State_.AUDIO_RECEIVED;
  view.setReadyForSpeech();
};

/**
 * Function is called when the user starts speaking.
 * @param {Event=} opt_event Emitted event for speech start.
 * @private
 */
speech.handleRecognitionSpeechStart_ = function(opt_event) {
  speech.resetIdleTimer_(speech.IDLE_TIMEOUT_MS_);
  speech.currentState_ = speech.State_.SPEECH_RECEIVED;
  view.setReceivingSpeech();
};

/**
 * Processes the recognition results arriving from the Web Speech API.
 * @param {SpeechRecognitionEvent} responseEvent Event coming from the API.
 * @private
 */
speech.handleRecognitionResult_ = function(responseEvent) {
  speech.resetIdleTimer_(speech.IDLE_TIMEOUT_MS_);

  switch (speech.currentState_) {
    case speech.State_.RESULT_RECEIVED:
    case speech.State_.SPEECH_RECEIVED:
      // Normal, expected states for processing results.
      break;
    case speech.State_.AUDIO_RECEIVED:
      // Network bugginess (the onaudiostart packet was lost).
      speech.handleRecognitionSpeechStart_();
      break;
    case speech.State_.STARTED:
      // Network bugginess (the onspeechstart packet was lost).
      speech.handleRecognitionAudioStart_();
      speech.handleRecognitionSpeechStart_();
      break;
    default:
      // Not expecting results in any other states.
      return;
  }

  const results = responseEvent.results;
  if (results.length == 0) {
    return;
  }
  speech.currentState_ = speech.State_.RESULT_RECEIVED;
  speech.interimResult_ = '';
  speech.finalResult_ = '';

  const finalResult = results[responseEvent.resultIndex];
  // Process final results.
  if (finalResult.isFinal) {
    speech.finalResult_ = finalResult[0].transcript;
    view.updateSpeechResult(speech.finalResult_, speech.finalResult_);

    speech.submitFinalResult_();
    return;
  }

  // Process interim results.
  for (let j = 0; j < results.length; j++) {
    const result = results[j][0];
    speech.interimResult_ += result.transcript;
    if (result.confidence > speech.RECOGNITION_CONFIDENCE_THRESHOLD_) {
      speech.finalResult_ += result.transcript;
    }
  }
  view.updateSpeechResult(speech.interimResult_, speech.finalResult_);

  // Force-stop long queries.
  if (speech.interimResult_.length > speech.QUERY_LENGTH_LIMIT_) {
    if (speech.finalResult_) {
      speech.submitFinalResult_();
    } else {
      speech.onErrorReceived_(RecognitionError.NO_MATCH);
    }
  }
};

/**
 * Convert a |RecognitionError| to a |LOG_TYPE| error constant,
 * for UMA logging.
 * @param {RecognitionError} error The received error.
 * @private
 */
speech.errorToLogType_ = function(error) {
  switch (error) {
    case RecognitionError.ABORTED:
      return LOG_TYPE.ERROR_ABORTED;
    case RecognitionError.AUDIO_CAPTURE:
      return LOG_TYPE.ERROR_AUDIO_CAPTURE;
    case RecognitionError.BAD_GRAMMAR:
      return LOG_TYPE.ERROR_BAD_GRAMMAR;
    case RecognitionError.LANGUAGE_NOT_SUPPORTED:
      return LOG_TYPE.ERROR_LANGUAGE_NOT_SUPPORTED;
    case RecognitionError.NETWORK:
      return LOG_TYPE.ERROR_NETWORK;
    case RecognitionError.NO_MATCH:
      return LOG_TYPE.ERROR_NO_MATCH;
    case RecognitionError.NO_SPEECH:
      return LOG_TYPE.ERROR_NO_SPEECH;
    case RecognitionError.NOT_ALLOWED:
      return LOG_TYPE.ERROR_NOT_ALLOWED;
    case RecognitionError.SERVICE_NOT_ALLOWED:
      return LOG_TYPE.ERROR_SERVICE_NOT_ALLOWED;
    default:
      return LOG_TYPE.ERROR_OTHER;
  }
};

/**
 * Handles state transition for the controller when an error occurs
 * during speech recognition.
 * @param {RecognitionError} error The appropriate error state from
 *     the RecognitionError enum.
 * @private
 */
speech.onErrorReceived_ = function(error) {
  speech.logEvent(speech.errorToLogType_(error));
  speech.resetIdleTimer_(speech.IDLE_TIMEOUT_MS_);
  speech.errorTimeoutMs_ = speech.getRecognitionErrorTimeout_(error);
  if (error != RecognitionError.ABORTED) {
    speech.currentState_ = speech.State_.ERROR_RECEIVED;
    view.showError(error);
    window.clearTimeout(speech.idleTimer_);
    speech.resetErrorTimer_(speech.errorTimeoutMs_);
  }
};

/**
 * Called when an error from Web Speech API is received.
 * @param {SpeechRecognitionError} error The error event.
 * @private
 */
speech.handleRecognitionError_ = function(error) {
  speech.onErrorReceived_(speech.getRecognitionError_(error.error));
};

/**
 * Stops speech recognition when no matches are found.
 * @private
 */
speech.handleRecognitionOnNoMatch_ = function() {
  speech.onErrorReceived_(RecognitionError.NO_MATCH);
};

/**
 * Stops the UI when the Web Speech API reports that it has halted speech
 * recognition.
 * @private
 */
speech.handleRecognitionEnd_ = function() {
  window.clearTimeout(speech.idleTimer_);

  let error;
  switch (speech.currentState_) {
    case speech.State_.STARTED:
      error = RecognitionError.AUDIO_CAPTURE;
      break;
    case speech.State_.AUDIO_RECEIVED:
      error = RecognitionError.NO_SPEECH;
      break;
    case speech.State_.SPEECH_RECEIVED:
    case speech.State_.RESULT_RECEIVED:
      error = RecognitionError.NO_MATCH;
      break;
    case speech.State_.ERROR_RECEIVED:
      error = RecognitionError.OTHER;
      break;
    default:
      return;
  }

  // If error has not yet been displayed.
  if (speech.currentState_ != speech.State_.ERROR_RECEIVED) {
    view.showError(error);
    speech.resetErrorTimer_(speech.ERROR_TIMEOUT_LONG_MS_);
  }
  speech.currentState_ = speech.State_.STOPPED;
};

/**
 * Determines whether the user's browser is probably running on a Mac.
 * @return {boolean} True iff the user's browser is running on a Mac.
 * @private
 */
speech.isUserAgentMac_ = function() {
  return window.navigator.userAgent.includes('Macintosh');
};

/**
 * Determines, if the given KeyboardEvent |code| is a space or enter key.
 * @param {string} code A KeyboardEvent's |code| property.
 * @return True, iff the code represents a space or enter key.
 * @private
 */
speech.isSpaceOrEnter_ = function(code) {
  switch (code) {
    case KEYCODE.ENTER:
    case KEYCODE.NUMPAD_ENTER:
    case KEYCODE.SPACE:
      return true;
    default:
      return false;
  }
};

/**
 * Determines if the given event's target id is for a button or navigation link.
 * @param {string} id An event's target id.
 * @return True, iff the id is for a button or link.
 * @private
 */
speech.isButtonOrLink_ = function(id) {
  switch (id) {
    case text.RETRY_LINK_ID:
    case text.SUPPORT_LINK_ID:
    case view.CLOSE_BUTTON_ID:
      return true;
    default:
      return false;
  }
};

/**
 * Handles the following keyboard actions.
 * - <CTRL> + <SHIFT> + <.> starts voice input(<CMD> + <SHIFT> + <.> on mac).
 * - <ESC> aborts voice input when the recognition interface is active.
 * - <ENTER> or <SPACE> interprets as a click if the target is a button or
 *   navigation link, otherwise it submits the speech query if there is one
 * @param {!Event} event The keydown event.
 */
speech.onKeyDown = function(event) {
  if (speech.isUiDefinitelyHidden_()) {
    const ctrlKeyPressed =
        event.ctrlKey || (speech.isUserAgentMac_() && event.metaKey);
    if (speech.currentState_ == speech.State_.READY &&
        event.code == KEYCODE.PERIOD && event.shiftKey && ctrlKeyPressed) {
      speech.logEvent(LOG_TYPE.ACTION_ACTIVATE_KEYBOARD);
      speech.start();
    }
  } else {
    // Ensures that keyboard events are not propagated during voice input.
    event.stopPropagation();

    if (event.code == KEYCODE.TAB) {
      speech.usingKeyboardNavigation_ = true;
    } else if (speech.isSpaceOrEnter_(event.code)) {
      if (event.target != null && speech.isButtonOrLink_(event.target.id)) {
        view.onWindowClick_(event);
      } else if (speech.finalResult_) {
        speech.submitFinalResult_();
      } else {
        speech.logEvent(LOG_TYPE.ACTION_CLOSE_OVERLAY);
        speech.stop();
      }
    } else if (event.code == KEYCODE.ESC) {
      speech.logEvent(LOG_TYPE.ACTION_CLOSE_OVERLAY);
      speech.stop();
    }
  }
};

/**
 * Displays the no match error if no interactions occur after some time while
 * the interface is active. This is a safety net in case the onend event
 * doesn't fire, or the user has persistent noise in the background, and does
 * not speak. If a high confidence transcription was received, then this submits
 * the search query instead of displaying an error.
 * @private
 */
speech.onIdleTimeout_ = function() {
  if (speech.finalResult_) {
    speech.submitFinalResult_();
    return;
  }

  switch (speech.currentState_) {
    case speech.State_.STARTED:
    case speech.State_.AUDIO_RECEIVED:
    case speech.State_.SPEECH_RECEIVED:
    case speech.State_.RESULT_RECEIVED:
    case speech.State_.ERROR_RECEIVED:
      speech.onErrorReceived_(RecognitionError.NO_MATCH);
      break;
  }
};

/**
 * Aborts the speech recognition interface when the user switches to a new
 * tab or window.
 * @private
 */
speech.onVisibilityChange_ = function() {
  if (speech.isUiDefinitelyHidden_()) {
    return;
  }

  if (document.webkitHidden) {
    speech.stop();
  }
};

/**
 * Aborts the speech session if the UI is showing and omnibox gets focused. Does
 * not abort if the user is using keyboard navigation (i.e. tab).
 */
speech.onOmniboxFocused = function() {
  if (!speech.isUiDefinitelyHidden_() && !speech.usingKeyboardNavigation_) {
    speech.logEvent(LOG_TYPE.ACTION_CLOSE_OVERLAY);
    speech.stop();
  }
};

/**
 * Change the location of this tab to the new URL. Used for query submission.
 * @param {!URL} url The URL to navigate to.
 * @private
 */
speech.navigateToUrl_ = function(url) {
  window.location.href = url.href;
};

/**
 * Submits the final spoken speech query to perform a search.
 * @private
 */
speech.submitFinalResult_ = function() {
  window.clearTimeout(speech.idleTimer_);
  if (!speech.finalResult_) {
    throw new Error('Submitting empty query.');
  }

  const searchParams = new URLSearchParams();
  // Add the encoded query. Getting |speech.finalResult_| needs to happen
  // before stopping speech.
  searchParams.append('q', speech.finalResult_);
  // Add a parameter to indicate that this request is a voice search.
  searchParams.append('gs_ivs', '1');

  // Build the query URL.
  const queryUrl = new URL('/search', speech.googleBaseUrl_);
  queryUrl.search = searchParams.toString();

  speech.logEvent(LOG_TYPE.ACTION_QUERY_SUBMITTED);
  speech.stop();
  speech.navigateToUrl_(queryUrl);
};

/**
 * Returns the error type based on the error string received from the webkit
 * speech recognition API.
 * @param {string} error The error string received from the webkit speech
 *     recognition API.
 * @return {RecognitionError} The appropriate error state from
 *     the RecognitionError enum.
 * @private
 */
speech.getRecognitionError_ = function(error) {
  switch (error) {
    case 'aborted':
      return RecognitionError.ABORTED;
    case 'audio-capture':
      return RecognitionError.AUDIO_CAPTURE;
    case 'bad-grammar':
      return RecognitionError.BAD_GRAMMAR;
    case 'language-not-supported':
      return RecognitionError.LANGUAGE_NOT_SUPPORTED;
    case 'network':
      return RecognitionError.NETWORK;
    case 'no-speech':
      return RecognitionError.NO_SPEECH;
    case 'not-allowed':
      return RecognitionError.NOT_ALLOWED;
    case 'service-not-allowed':
      return RecognitionError.SERVICE_NOT_ALLOWED;
    default:
      return RecognitionError.OTHER;
  }
};

/**
 * Returns a timeout based on the error received from the webkit speech
 * recognition API.
 * @param {RecognitionError} error An error from the RecognitionError enum.
 * @return {number} The appropriate timeout duration for displaying the error.
 * @private
 */
speech.getRecognitionErrorTimeout_ = function(error) {
  switch (error) {
    case RecognitionError.AUDIO_CAPTURE:
    case RecognitionError.NO_SPEECH:
    case RecognitionError.NOT_ALLOWED:
    case RecognitionError.SERVICE_NOT_ALLOWED:
    case RecognitionError.NO_MATCH:
      return speech.ERROR_TIMEOUT_LONG_MS_;
    default:
      return speech.ERROR_TIMEOUT_SHORT_MS_;
  }
};

/**
 * Resets the idle state timeout.
 * @param {number} duration The duration after which to close the UI.
 * @private
 */
speech.resetIdleTimer_ = function(duration) {
  window.clearTimeout(speech.idleTimer_);
  speech.idleTimer_ = window.setTimeout(speech.onIdleTimeout_, duration);
};

/**
 * Resets the idle error state timeout.
 * @param {number} duration The duration after which to close the UI during an
 *     error state.
 * @private
 */
speech.resetErrorTimer_ = function(duration) {
  window.clearTimeout(speech.errorTimer_);
  speech.errorTimer_ = window.setTimeout(speech.stop, duration);
};

/**
 * Check to see if the speech recognition interface is running, and has
 * received any results.
 * @return {boolean} True, if the speech recognition interface is running,
 *     and has received any results.
 */
speech.hasReceivedResults = function() {
  return speech.currentState_ == speech.State_.RESULT_RECEIVED;
};

/**
 * Check to see if the speech recognition interface is running.
 * @return {boolean} True, if the speech recognition interface is running.
 */
speech.isRecognizing = function() {
  switch (speech.currentState_) {
    case speech.State_.STARTED:
    case speech.State_.AUDIO_RECEIVED:
    case speech.State_.SPEECH_RECEIVED:
    case speech.State_.RESULT_RECEIVED:
      return true;
  }
  return false;
};

/**
 * Check if the controller is in a state where the UI is definitely hidden.
 * Since we show the UI for a few seconds after we receive an error from the
 * API, we need a separate definition to |speech.isRecognizing()| to indicate
 * when the UI is hidden. <strong>Note:</strong> that if this function
 * returns false, it might not necessarily mean that the UI is visible.
 * @return {boolean} True if the UI is hidden.
 * @private
 */
speech.isUiDefinitelyHidden_ = function() {
  switch (speech.currentState_) {
    case speech.State_.READY:
    case speech.State_.UNINITIALIZED:
      return true;
  }
  return false;
};

/**
 * Handles click events during speech recognition.
 * @param {boolean} shouldSubmit True if a query should be submitted.
 * @param {boolean} shouldRetry True if the interface should be restarted.
 * @param {boolean} navigatingAway True if the browser is navigating away
 *     from the NTP.
 * @private
 */
speech.onClick_ = function(shouldSubmit, shouldRetry, navigatingAway) {
  if (speech.finalResult_ && shouldSubmit) {
    speech.submitFinalResult_();
  } else if (speech.currentState_ == speech.State_.STOPPED && shouldRetry) {
    speech.reset_();
    speech.start();
  } else if (speech.currentState_ == speech.State_.STOPPED && navigatingAway) {
    // If the user clicks on a "Learn more" or "Details" support page link
    // from an error message, do nothing, and let Chrome navigate to that page.
  } else {
    speech.logEvent(LOG_TYPE.ACTION_CLOSE_OVERLAY);
    speech.stop();
  }
};

/* TEXT VIEW */

/**
 * Provides methods for styling and animating the text areas
 * left of the microphone button.
 */
const text = {};

/**
 * ID for the "Try Again" link shown in error output.
 * @const
 */
text.RETRY_LINK_ID = 'voice-retry-link';

/**
 * ID for the Voice Search support site link shown in error output.
 * @const
 */
text.SUPPORT_LINK_ID = 'voice-support-link';

/**
 * Class for the links shown in error output.
 * @const @private
 */
text.ERROR_LINK_CLASS_ = 'voice-text-link';

/**
 * Class name for the speech recognition result output area.
 * @const @private
 */
text.TEXT_AREA_CLASS_ = 'voice-text';

/**
 * Class name for the "Listening..." text animation.
 * @const @private
 */
text.LISTENING_ANIMATION_CLASS_ = 'listening-animation';

/**
 * ID of the final / high confidence speech recognition results element.
 * @const @private
 */
text.FINAL_TEXT_AREA_ID_ = 'voice-text-f';

/**
 * ID of the interim / low confidence speech recognition results element.
 * @const @private
 */
text.INTERIM_TEXT_AREA_ID_ = 'voice-text-i';

/**
 * The line height of the speech recognition results text.
 * @const @private
 */
text.LINE_HEIGHT_ = 1.2;

/**
 * Font size in the full page view in pixels.
 * @const @private
 */
text.FONT_SIZE_ = 32;

/**
 * Delay in milliseconds before showing the initializing message.
 * @const @private
 */
text.INITIALIZING_TIMEOUT_MS_ = 300;

/**
 * Delay in milliseconds before showing the listening message.
 * @const @private
 */
text.LISTENING_TIMEOUT_MS_ = 2000;

/**
 * Base link target for help regarding voice search. To be appended
 * with a locale string for proper target site localization.
 * @const @private
 */
text.SUPPORT_LINK_BASE_ =
    'https://support.google.com/chrome/?p=ui_voice_search&hl=';

/**
 * The final / high confidence speech recognition result element.
 * @private {Element}
 */
text.final_;

/**
 * The interim / low confidence speech recognition result element.
 * @private {Element}
 */
text.interim_;

/**
 * Stores the ID of the initializing message timer.
 * @private {number}
 */
text.initializingTimer_;

/**
 * Stores the ID of the listening message timer.
 * @private {number}
 */
text.listeningTimer_;

/**
 * Finds the text view elements.
 */
text.init = function() {
  text.final_ = $(text.FINAL_TEXT_AREA_ID_);
  text.interim_ = $(text.INTERIM_TEXT_AREA_ID_);
  text.clear();
};

/**
 * Updates the text elements with new recognition results.
 * @param {string} interimText Low confidence speech recognition result text.
 * @param {string} opt_finalText High confidence speech recognition result
 *     text, defaults to an empty string.
 */
text.updateTextArea = function(interimText, opt_finalText = '') {
  window.clearTimeout(text.initializingTimer_);
  text.clearListeningTimeout();

  text.interim_.textContent = interimText;
  text.final_.textContent = opt_finalText;

  text.interim_.className = text.final_.className = text.getTextClassName_();
};

/**
 * Sets the text view to the initializing state. The initializing message
 * shown while waiting for permission is not displayed immediately, but after
 * a short timeout. The reason for this is that the "Waiting..." message would
 * still appear ("blink") every time a user opens Voice Search, even if they
 * have already granted and persisted microphone permission for the NTP,
 * and could therefore directly proceed to the "Speak now" message.
 */
text.showInitializingMessage = function() {
  text.interim_.textContent = '';
  text.final_.textContent = '';

  const displayMessage = function() {
    if (text.interim_.textContent == '') {
      text.updateTextArea(speech.messages.waiting);
    }
  };
  text.initializingTimer_ =
      window.setTimeout(displayMessage, text.INITIALIZING_TIMEOUT_MS_);
};

/**
 * Sets the text view to the ready state.
 */
text.showReadyMessage = function() {
  window.clearTimeout(text.initializingTimer_);
  text.clearListeningTimeout();
  text.updateTextArea(speech.messages.ready);
  text.startListeningMessageAnimation_();
};

/**
 * Display an error message in the text area for the given error.
 * @param {RecognitionError} error The error that occured.
 */
text.showErrorMessage = function(error) {
  text.updateTextArea(text.getErrorMessage_(error));

  const linkElement = text.getErrorLink_(error);
  // Setting textContent removes all children (no need to clear link elements).
  if (linkElement) {
    text.interim_.textContent += ' ';
    text.interim_.appendChild(linkElement);
  }
};

/**
 * Returns an error message based on the error.
 * @param {RecognitionError} error The error that occured.
 * @private
 */
text.getErrorMessage_ = function(error) {
  switch (error) {
    case RecognitionError.NO_MATCH:
      return speech.messages.noTranslation;
    case RecognitionError.NO_SPEECH:
      return speech.messages.noVoice;
    case RecognitionError.AUDIO_CAPTURE:
      return speech.messages.audioError;
    case RecognitionError.NETWORK:
      return speech.messages.networkError;
    case RecognitionError.NOT_ALLOWED:
    case RecognitionError.SERVICE_NOT_ALLOWED:
      return speech.messages.permissionError;
    case RecognitionError.LANGUAGE_NOT_SUPPORTED:
      return speech.messages.languageError;
    default:
      return speech.messages.otherError;
  }
};

/**
 * Returns an error message help link based on the error.
 * @param {RecognitionError} error The error that occured.
 * @private
 */
text.getErrorLink_ = function(error) {
  const linkElement = document.createElement('a');
  linkElement.className = text.ERROR_LINK_CLASS_;

  switch (error) {
    case RecognitionError.NO_MATCH:
      linkElement.id = text.RETRY_LINK_ID;
      linkElement.tabIndex = '0';
      linkElement.textContent = speech.messages.tryAgain;
      // When clicked, |view.onWindowClick_| gets called.
      return linkElement;
    case RecognitionError.NO_SPEECH:
    case RecognitionError.AUDIO_CAPTURE:
      linkElement.id = text.SUPPORT_LINK_ID;
      linkElement.href = text.SUPPORT_LINK_BASE_ + getChromeUILanguage();
      linkElement.textContent = speech.messages.learnMore;
      linkElement.target = '_blank';
      return linkElement;
    case RecognitionError.NOT_ALLOWED:
    case RecognitionError.SERVICE_NOT_ALLOWED:
      linkElement.id = text.SUPPORT_LINK_ID;
      linkElement.href = text.SUPPORT_LINK_BASE_ + getChromeUILanguage();
      linkElement.textContent = speech.messages.details;
      linkElement.target = '_blank';
      return linkElement;
    default:
      return null;
  }
};

/**
 * Clears the text elements.
 */
text.clear = function() {
  text.updateTextArea('');

  text.clearListeningTimeout();
  window.clearTimeout(text.initializingTimer_);

  text.interim_.className = text.TEXT_AREA_CLASS_;
  text.final_.className = text.TEXT_AREA_CLASS_;
};

/**
 * Cancels listening message display.
 */
text.clearListeningTimeout = function() {
  window.clearTimeout(text.listeningTimer_);
};

/**
 * Determines the class name of the text output Elements.
 * @return {string} The class name.
 * @private
 */
text.getTextClassName_ = function() {
  // Shift up for every line.
  const oneLineHeight = text.LINE_HEIGHT_ * text.FONT_SIZE_ + 1;
  const twoLineHeight = text.LINE_HEIGHT_ * text.FONT_SIZE_ * 2 + 1;
  const threeLineHeight = text.LINE_HEIGHT_ * text.FONT_SIZE_ * 3 + 1;
  const fourLineHeight = text.LINE_HEIGHT_ * text.FONT_SIZE_ * 4 + 1;

  const height = text.interim_.scrollHeight;
  let className = text.TEXT_AREA_CLASS_;

  if (height > fourLineHeight) {
    className += ' voice-text-5l';
  } else if (height > threeLineHeight) {
    className += ' voice-text-4l';
  } else if (height > twoLineHeight) {
    className += ' voice-text-3l';
  } else if (height > oneLineHeight) {
    className += ' voice-text-2l';
  }
  return className;
};

/**
 * Displays the listening message animation after the ready message has been
 * shown for |text.LISTENING_TIMEOUT_MS_| milliseconds without further user
 * action.
 * @private
 */
text.startListeningMessageAnimation_ = function() {
  const animateListeningText = function() {
    // If speech is active with no results yet, show the message and animation.
    if (speech.isRecognizing() && !speech.hasReceivedResults()) {
      text.updateTextArea(speech.messages.listening);
      text.interim_.classList.add(text.LISTENING_ANIMATION_CLASS_);
    }
  };

  text.listeningTimer_ =
      window.setTimeout(animateListeningText, text.LISTENING_TIMEOUT_MS_);
};

/* END TEXT VIEW */

/* MICROPHONE VIEW */

/**
 * Provides methods for animating the microphone button and icon
 * on the Voice Search full screen overlay.
 */
const microphone = {};

/**
 * ID for the button Element.
 * @const
 */
microphone.RED_BUTTON_ID = 'voice-button';

/**
 * ID for the level animations Element that indicates input volume.
 * @const @private
 */
microphone.LEVEL_ID_ = 'voice-level';

/**
 * ID for the container of the microphone, red button and level animations.
 * @const @private
 */
microphone.CONTAINER_ID_ = 'voice-button-container';

/**
 * The minimum transform scale for the volume rings.
 * @const @private
 */
microphone.LEVEL_SCALE_MINIMUM_ = 0.5;

/**
 * The range of the transform scale for the volume rings.
 * @const @private
 */
microphone.LEVEL_SCALE_RANGE_ = 0.55;

/**
 * The minimum transition time (in milliseconds) for the volume rings.
 * @const @private
 */
microphone.LEVEL_TIME_STEP_MINIMUM_ = 170;

/**
 * The range of the transition time for the volume rings.
 * @const @private
 */
microphone.LEVEL_TIME_STEP_RANGE_ = 10;

/**
 * The button with the microphone icon.
 * @private {Element}
 */
microphone.button_;

/**
 * The voice level element that is displayed when the user starts speaking.
 * @private {Element}
 */
microphone.level_;

/**
 * Variable to indicate whether level animations are underway.
 * @private {boolean}
 */
microphone.isLevelAnimating_ = false;

/**
 * Creates/finds the output elements for the microphone rendering and animation.
 */
microphone.init = function() {
  // Get the button element and microphone container.
  microphone.button_ = $(microphone.RED_BUTTON_ID);

  // Get the animation elements.
  microphone.level_ = $(microphone.LEVEL_ID_);
};

/**
 * Starts the volume circles animations, if it has not started yet.
 */
microphone.startInputAnimation = function() {
  if (!microphone.isLevelAnimating_) {
    microphone.isLevelAnimating_ = true;
    microphone.runLevelAnimation_();
  }
};

/**
 * Stops the volume circles animations.
 */
microphone.stopInputAnimation = function() {
  microphone.isLevelAnimating_ = false;
};

/**
 * Runs the volume level animation.
 * @private
 */
microphone.runLevelAnimation_ = function() {
  if (!microphone.isLevelAnimating_) {
    microphone.level_.style.removeProperty('opacity');
    microphone.level_.style.removeProperty('transition');
    microphone.level_.style.removeProperty('transform');
    return;
  }
  const scale = microphone.LEVEL_SCALE_MINIMUM_ +
      Math.random() * microphone.LEVEL_SCALE_RANGE_;
  const timeStep = Math.round(
      microphone.LEVEL_TIME_STEP_MINIMUM_ +
      Math.random() * microphone.LEVEL_TIME_STEP_RANGE_);
  microphone.level_.style.setProperty(
      'transition', 'transform ' + timeStep + 'ms ease-in-out');
  microphone.level_.style.setProperty('transform', 'scale(' + scale + ')');
  window.setTimeout(microphone.runLevelAnimation_, timeStep);
};

/* END MICROPHONE VIEW */

/* VIEW */

/**
 * Provides methods for manipulating and animating the Voice Search
 * full screen overlay.
 */
const view = {};

/**
 * ID for the close button in the speech output container.
 * @const
 */
view.CLOSE_BUTTON_ID = 'voice-close-button';

/**
 * Class name of the speech recognition interface on the homepage.
 * @const @private
 */
view.OVERLAY_CLASS_ = 'overlay';

/**
 * Class name of the speech recognition interface when it is hidden on the
 * homepage.
 * @const @private
 */
view.OVERLAY_HIDDEN_CLASS_ = 'overlay-hidden';

/**
 * ID for the dialog that contains the speech recognition interface.
 * @const @private
 */
view.DIALOG_ID_ = 'voice-overlay-dialog';

/**
 * ID for the speech output background.
 * @const @private
 */
view.BACKGROUND_ID_ = 'voice-overlay';

/**
 * ID for the speech output container.
 * @const @private
 */
view.CONTAINER_ID_ = 'voice-outer';

/**
 * Class name used to modify the UI to the 'listening' state.
 * @const @private
 */
view.MICROPHONE_LISTENING_CLASS_ = 'outer voice-ml';

/**
 * Class name used to modify the UI to the 'receiving speech' state.
 * @const @private
 */
view.RECEIVING_SPEECH_CLASS_ = 'outer voice-rs';

/**
 * Class name used to modify the UI to the 'error received' state.
 * @const @private
 */
view.ERROR_RECEIVED_CLASS_ = 'outer voice-er';

/**
 * Class name used to modify the UI to the inactive state.
 * @const @private
 */
view.INACTIVE_CLASS_ = 'outer';

/**
 * Background element and container of all other elements.
 * @private {Element}
 */
view.background_;

/**
 * The container used to position the microphone and text output area.
 * @private {Element}
 */
view.container_;

/**
 * True if the the last error message shown was for the 'no-match' error.
 * @private {boolean}
 */
view.isNoMatchShown_ = false;

/**
 * True if the UI elements are visible.
 * @private {boolean}
 */
view.isVisible_ = false;

/**
 * The function to call when there is a click event.
 * @private {Function}
 */
view.onClick_;

/**
 * Displays the UI.
 */
view.show = function() {
  if (!view.isVisible_) {
    text.showInitializingMessage();
    view.showView_();
    window.addEventListener('click', view.onWindowClick_, false);
  }
};

/**
 * Sets the output area text to listening. This should only be called when
 * the Web Speech API starts receiving audio input (i.e., onaudiostart).
 */
view.setReadyForSpeech = function() {
  if (view.isVisible_) {
    view.container_.className = view.MICROPHONE_LISTENING_CLASS_;
    text.showReadyMessage();
  }
};

/**
 * Shows the pulsing animation emanating from the microphone. This should only
 * be called when the Web Speech API starts receiving speech input (i.e.,
 * |onspeechstart|). Do note that this may also be run when the Web Speech API
 * is receiving speech recognition results (|onresult|), because |onspeechstart|
 * may not have been called.
 */
view.setReceivingSpeech = function() {
  if (view.isVisible_) {
    view.container_.className = view.RECEIVING_SPEECH_CLASS_;
    microphone.startInputAnimation();
    text.clearListeningTimeout();
  }
};

/**
 * Updates the speech recognition results output with the latest results.
 * @param {string} interimResultText Low confidence recognition text (grey).
 * @param {string} finalResultText High confidence recognition text (black).
 */
view.updateSpeechResult = function(interimResultText, finalResultText) {
  if (view.isVisible_) {
    // If the Web Speech API is receiving speech recognition results
    // (|onresult|) and |onspeechstart| has not been called.
    if (view.container_.className != view.RECEIVING_SPEECH_CLASS_) {
      view.setReceivingSpeech();
    }
    text.updateTextArea(interimResultText, finalResultText);
  }
};

/**
 * Hides the UI and stops animations.
 */
view.hide = function() {
  window.removeEventListener('click', view.onWindowClick_, false);
  view.stopMicrophoneAnimations_();
  view.hideView_();
  view.isNoMatchShown_ = false;
  text.clear();
};

/**
 * Find the page elements that will be used to render the speech recognition
 * interface area.
 * @param {Function} onClick The function to call when there is a click event
 *    in the window.
 */
view.init = function(onClick) {
  view.onClick_ = onClick;

  view.dialog_ = $(view.DIALOG_ID_);
  view.background_ = $(view.BACKGROUND_ID_);
  view.container_ = $(view.CONTAINER_ID_);

  text.init();
  microphone.init();
};

/**
 * Sets accessibility titles/labels for the page elements.
 * @param {!Object} translatedStrings Dictionary of localized title strings.
 */
view.setTitles = function(translatedStrings) {
  const closeButton = $(view.CLOSE_BUTTON_ID);
  closeButton.title = translatedStrings.voiceCloseTooltip;
  closeButton.setAttribute('aria-label', translatedStrings.voiceCloseTooltip);
};

/**
 * Displays an error message and stops animations.
 * @param {RecognitionError} error The error type.
 */
view.showError = function(error) {
  view.container_.className = view.ERROR_RECEIVED_CLASS_;
  text.showErrorMessage(error);
  view.stopMicrophoneAnimations_();
  view.isNoMatchShown_ = (error == RecognitionError.NO_MATCH);
};

/**
 * Makes the view visible.
 * @private
 */
view.showView_ = function() {
  if (!view.isVisible_) {
    view.dialog_.showModal();
    view.background_.className = view.OVERLAY_HIDDEN_CLASS_;
    view.background_.className = view.OVERLAY_CLASS_;
    view.isVisible_ = true;
  }
};

/**
 * Hides the view.
 * @private
 */
view.hideView_ = function() {
  view.dialog_.close();
  view.background_.className = view.OVERLAY_HIDDEN_CLASS_;
  view.container_.className = view.INACTIVE_CLASS_;
  view.background_.removeAttribute('style');
  view.isVisible_ = false;
};

/**
 * Stops the animations in the microphone view.
 * @private
 */
view.stopMicrophoneAnimations_ = function() {
  microphone.stopInputAnimation();
};

/**
 * Makes sure that a click anywhere closes the UI when it is active.
 * @param {!Event} event The click event.
 * @private
 */
view.onWindowClick_ = function(event) {
  if (!view.isVisible_) {
    return;
  }
  const retryLinkClicked = event.target.id === text.RETRY_LINK_ID;
  const supportLinkClicked = event.target.id === text.SUPPORT_LINK_ID;
  const micIconClicked = event.target.id === microphone.RED_BUTTON_ID;

  const submitQuery = micIconClicked && !view.isNoMatchShown_;
  const shouldRetry =
      retryLinkClicked || (micIconClicked && view.isNoMatchShown_);
  const navigatingAway = supportLinkClicked;

  speech.usingKeyboardNavigation_ = false;

  if (shouldRetry) {
    if (micIconClicked) {
      speech.logEvent(LOG_TYPE.ACTION_TRY_AGAIN_MIC_BUTTON);
    } else if (retryLinkClicked) {
      speech.logEvent(LOG_TYPE.ACTION_TRY_AGAIN_LINK);
    }
  }
  if (supportLinkClicked) {
    speech.logEvent(LOG_TYPE.ACTION_SUPPORT_LINK_CLICKED);
  }

  view.onClick_(submitQuery, shouldRetry, navigatingAway);
};

/* END VIEW */
