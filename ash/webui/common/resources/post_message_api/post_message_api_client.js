// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {RequestHandler} from './post_message_api_request_handler.js';

/**
 * Creates a new JavaScript native Promise and captures its resolve and reject
 * callbacks. The promise, resolve, and reject are available as properties
 * @final
 * @template T
 */
class NativeResolver {
  constructor() {
    /** @type {function((T|!IThenable<T>|!Thenable)=)} */
    this.resolve;
    /** @type {function(*=)} */
    this.reject;

    /** @type {!Promise<T>} */
    this.promise = new Promise((resolve, reject) => {
      this.resolve = resolve;
      this.reject = reject;
    });
  }
}

/**
 * Class that provides the functionality for talking to a PostMessageApiServer
 * over the postMessage API.  This should be subclassed and the subclass should
 * expose methods that are implemented by the server. The following is an
 * example.
 * class FooClient extends PostMessageApiClient {
 *  ...
 *   doFoo(args) {
 *    return this.callApiFn('foo', args);
 *   }
 * }
 *
 */
export class PostMessageApiClient {
  /**
   * @param {!string} serverOriginURLFilter  Only messages from this origin
   *     will be accepted.
   * @param {?Window} targetWindow, If the connection is already established,
   *     then provide the target window.
   */
  constructor(serverOriginURLFilter, targetWindow) {
    /**
     * @private @const {!string} Filter to use to validate
     * the origin of received messages.  The origin of messages
     * must exactly match this value.
     */
    this.serverOriginURLFilter_ = serverOriginURLFilter;

    /*
     * @private {number}
     */
    this.nextMethodId_ = 0;
    /**
     * Map of methods awaiting a response.
     * @private {!Map<number, !NativeResolver>}
     */
    this.methodsAwaitingResponse_ = new Map();

    /**
     * The parent window.
     * @private {?Window}
     */
    this.targetWindow_ = targetWindow;

    /**
     * The request handler for the client.
     * @private {RequestHandler}
     */
    this.requestHandler_ = null;

    /**
     * Function property that tracks whether client has
     * been initialized by the server.
     * @private {Function}
     */
    this.boundOnInitialize_ = null;
    if (this.targetWindow_ === null) {
      // Wait for an init message from the server.
      this.boundOnInitialize_ = (event) => {
        this.onInitialize_(event);
      };
      window.addEventListener('message', this.boundOnInitialize_);
    } else {
      // When trying to bootstrap a duplex communication channel, the instance
      // of PostMessageAPIServer may create a PostMessageAPIClient and pass the
      // window of the element it has already established connection with. In
      // this case, the new PostMessageAPIClient doesn't need to wait for 'init'
      // messages and can start sending requests immediately.
      window.addEventListener('message', (event) => {
        this.onMessage_(event);
      });
    }
  }

  /**
   * Virtual method called when the client is initialized and it knows the
   * server that it is communicating with. This method should be overwritten by
   * subclasses which would like to know when initialization is done.
   */
  onInitialized() {}

  /**
   * Returns if the client's connection to its handler is initialized or not.
   * @return {boolean}
   */
  isInitialized() {
    return this.targetWindow_ !== null;
  }

  //
  // Private implementation:
  //

  /**
   * Handles initialization event sent from the server to establish
   * communication.
   * @private
   * @param {!Event} event  An event received when the initialization message is
   *     sent from the server.
   */
  onInitialize_(event) {
    if (!this.originMatchesFilter(event.origin)) {
      console.error(
          'Initialization event received from non-authorized origin: ' +
          event.origin);
      return;
    }
    this.targetWindow_ = event.source;
    this.targetWindow_.postMessage('init', this.serverOriginURLFilter_);
    window.removeEventListener('message', this.boundOnInitialize_);
    this.boundOnInitialize_ = null;
    window.addEventListener('message', (incoming_event) => {
      this.onMessage_(incoming_event);
    });
    this.onInitialized();
  }

  /**
   * Determine if the specified server origin URL matches the origin filter.
   * @param {!string} origin The origin URL to match with the filter.
   * @return {boolean}  whether the specified origin matches the filter.
   */
  originMatchesFilter(origin) {
    return (new URL(origin)).toString() === this.serverOriginURLFilter_;
  }

  /**
   * Handles postMessage events sent from the server.
   * @param {Event} event  An event received from the server via the postMessage
   *     API.
   */
  async onMessage_(event) {
    if (!this.originMatchesFilter(event.origin)) {
      console.error(
          'Message received from non-authorized origin: ' + event.origin);
      return;
    }

    if (event.source !== this.targetWindow_) {
      console.error('discarding event whose source is not the parent window');
      return;
    }

    // If there is a function call associated with the message being received,
    // then it is intended for the RequestHandler. Therefore, return early.
    if (event.data.fn) {
      return;
    }

    if (!this.methodsAwaitingResponse_.has(event.data.methodId)) {
      console.info('discarding event method is not waiting for a response');
      return;
    }

    const resolver = this.methodsAwaitingResponse_.get(event.data.methodId);
    this.methodsAwaitingResponse_.delete(event.data.methodId);
    if (event.data.rejected) {
      resolver.reject(event.data.error);
    } else {
      resolver.resolve(event.data.result);
    }
  }

  /**
   * Converts a function call with arguments into a postMessage event
   * and sends it to the server via the postMessage API.
   * @param {string} fn  The function to call.
   * @param {!Array<Object>} args The arguments to pass to the function.
   * @return {!Promise} A promise capturing the executing of the function.
   */
  callApiFn(fn, args) {
    if (!this.targetWindow_) {
      return Promise.reject('No parent window defined');
    }

    const newMethodId = this.nextMethodId_++;
    const resolver = new NativeResolver();
    this.targetWindow_.postMessage(
        {
          methodId: newMethodId,
          fn: fn,
          args: args,
        },
        this.serverOriginURLFilter_);

    this.methodsAwaitingResponse_.set(newMethodId, resolver);

    return resolver.promise;
  }
}
