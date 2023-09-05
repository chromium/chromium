// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {RequestHandler} from './post_message_api_request_handler.js';

/**
 *  Initialization retry wait in milliseconds (subject to exponential backoff).
 */
const INITIALIZATION_ATTEMPT_RETRY_WAIT_MS = 100;

/**
 * Maximum number of initialization attempts before resetting the
 * initialization attempt cycle.  With exponential backoff, this works out
 * a maximum wait of 25 seconds on the 8th attempt before restarting.
 */
const MAX_INITIALIZATION_ATTEMPTS = 8;

/**
 * Class that provides the functionality for talking to a client
 * over the PostMessageAPI.  This should be subclassed and the subclass should
 * provide supported methods.
 */
export class PostMessageApiServer extends RequestHandler {
  constructor(clientElement, targetURL, messageOriginURLFilter) {
    super(clientElement, messageOriginURLFilter, targetURL);
    /**
     *  The ID of the timeout set before checking whether initialization has
     * taken place yet.
     * @private {number}
     */
    this.initializationTimeoutId_ = 0;

    /**
     * Indicates how many attempts have been made to initialize the channel.
     * @private {number}
     */
    this.numInitializationAttempts_ = 0;

    /**
     * Indicates whether the communication channel between this server and the
     * WebView has been established.
     * @private {boolean}
     */
    this.isInitialized_ = false;

    if (this.clientElement().tagName === 'IFRAME') {
      this.clientElement().onload = () => {
        this.onLoad_();
      };
    } else {
      this.clientElement().addEventListener('contentload', () => {
        this.onLoad_();
      });
    }

    // Listen for events.
    window.addEventListener('message', (event) => {
      this.onMessage_(event);
    });
  }

  /**
   * Send initialization message to client element.
   */
  initialize() {
    if (this.isInitialized_ ||
        !this.originMatchesFilter(this.clientElement().src)) {
      return;
    }

    if (this.numInitializationAttempts_ < MAX_INITIALIZATION_ATTEMPTS) {
      // Tell the embedded webviews whose src matches our origin to initialize
      // by sending it a message, which will include a handle for it to use to
      // send messages back.
      console.info(
          'Sending init message to guest content,  attempt # :' +
          this.numInitializationAttempts_);

      this.targetWindow().postMessage('init', this.targetUrl().toString());

      // Set timeout to check if initialization message has been received using
      // exponential backoff.
      this.initializationTimeoutId_ = setTimeout(
          () => {
            // If the timeout id is non-zero, that indicates that initialization
            // hasn't succeeded yet, so  try to initialize again.
            this.initialize();
          },
          INITIALIZATION_ATTEMPT_RETRY_WAIT_MS *
              (2 ** this.numInitializationAttempts_));

      this.numInitializationAttempts_++;
    } else {
      // Exponential backoff has maxed out. Show error page if present.
      this.onInitializationError(this.clientElement().src);
    }
  }

  /**
   *  Virtual method to be overridden by implementations of this class to notify
   * them that we were unable to initialize communication channel with the
   * `this.clientElement()`.
   *
   * @param {!string} origin The origin URL that was not able to initialize
   *     communication.
   */
  onInitializationError(origin) {}

  /**
   * Virtual method to be overridden by implementation of this class to notify
   * them that communication has successfully been initialized with the client
   * element.
   */
  onInitializationComplete() {}

  /**
   * Handles postMessage events from the client.
   * @private
   * @param {Event} event  The postMessage event to handle.
   */
  async onMessage_(event) {
    if (!this.originMatchesFilter(event.origin)) {
      console.info(
          'Message received from unauthorized origin: ' + event.origin);
      return;
    }

    if (event.data === 'init') {
      if (this.initializationTimeoutId_) {
        // Cancel the current init timeout, and signal to the initialization
        // polling process that we have received an init message from the guest
        // content, so it doesn't reschedule the timer.
        clearTimeout(this.initializationTimeoutId_);
        this.initializationTimeoutId_ = 0;
      }

      this.isInitialized_ = true;
      this.onInitializationComplete();
      return;
    }
  }

  /**
   * Reinitiates the connection when the content inside the clientElement
   * reloads.
   * @private
   */
  onLoad_() {
    this.numInitializationAttempts_ = 0;
    this.isInitialized_ = false;
    this.initialize();
  }
}
