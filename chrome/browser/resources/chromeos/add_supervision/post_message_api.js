// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Class that provides the functionality for talking to a client
 * over the PostMessageAPI.  This should be subclassed and the
 * methods provided in methodList should be implemented as methods
 * of the subclass.
 */
class PostMessageAPIServer {
  constructor(clientElement, methodList, targetURL, messageOriginURLFilter) {
    /**
     * The Window type element to which this server will listen for messages,
     * probably a <webview>, but also could be an <iframe> or a browser window
     * object.
     * @private @const {!Element}
     */
    this.clientElement_ = clientElement;

    /**
     * The guest URL embedded in the element above. Used for message targeting.
     * This should be same as the URL loaded in the clientElement, i.e. the
     * "src" attribute of a <webview>.
     * @private @const {!URL}
     */
    this.targetURL_ = new URL(targetURL);

    /**
     * Incoming messages received from origin URLs without this prefix
     * will not be accepted. This should be used to restrict the API access
     * to the intended guest content.
     * @private @const {!URL}
     */
    this.messageOriginURLFilter_ = new URL(messageOriginURLFilter);

    /**
     * Map that stores references to the methods implemented by the API.
     * @private {!Map<string, function(!Array): (Promise|undefined)>}
     */
    this.apiFns_ = new Map();

    // Listen for the embedding element to finish loading, then
    // tell it to initialize by sending it a message, which will include
    // a handle for it to use to send messages back.
    this.clientElement_.addEventListener('contentload', () => {
      this.clientElement_.contentWindow.postMessage(
          'init', this.targetURL_.toString());
    });

    // Listen for events.
    window.addEventListener('message', (event) => {
      this.onMessage_(event);
    });
  }

  /**
   * Registers the specified method name with the specified
   * function.
   *
   * @param {!string} methodName name of the method to register.
   * @param {!function(!Array): (Promise|undefined)} method The function to
   *     associate with the name.
   */
  registerMethod(methodName, method) {
    this.apiFns_.set(methodName, method);
  }

  /**
   * Determines if the specified origin matches the origin filter.
   * @private
   * @param {!string} origin The origin URL to match with the filter.
   * @return {boolean}  whether the specified origin matches the filter.
   */
  originMatchesFilter_(origin) {
    const originURL = new URL(origin);

    // We allow the pathname portion of the URL to be a prefix filter,
    // to permit for different paths communicating with this server.
    return originURL.protocol == this.messageOriginURLFilter_.protocol &&
        originURL.host == this.messageOriginURLFilter_.host &&
        originURL.pathname.startsWith(this.messageOriginURLFilter_.pathname);
  }

  /**
   * Handles postMessage events from the client.
   * @private
   * @param {Event} event  The postMessage event to handle.
   */
  onMessage_(event) {
    if (!this.originMatchesFilter_(event.origin)) {
      console.error(
          'Message received from unauthorized origin: ' + event.origin);
      return;
    }
    const methodId = event.data.methodId;
    const fn = event.data.fn;
    const args = event.data.args || [];

    if (!this.apiFns_.has(fn)) {
      console.error('Unknown function requested: ' + fn);
      return;
    }

    const sendMessage = (methodId, result) => {
      this.clientElement_.contentWindow.postMessage(
          {
            methodId: methodId,
            result: result,
          },
          this.targetURL_.toString());
    };

    // Some methods return a promise and some don't. If we have a promise,
    // we resolve it first, otherwise we send the result directly (e.g., for
    // void functions we send 'undefined').
    const result = this.apiFns_.get(fn)(args);
    if (result instanceof Promise) {
      result.then((result) => sendMessage(methodId, result));
    } else {
      sendMessage(methodId, result);
    }
  }
}

/**
 * Class that provides the functionality for talking to a PostMessageAPIServer
 * over the postMessage API.  This should be subclassed and the methods in the
 * server that the client needs to access should be provided in methodList.
 */
class PostMessageAPIClient {
  /**
   * @param {!Array<string>} methodList The list of methods accessible via the
   *     client.
   * @param {!string} serverOriginURLFilter  Only messages from this origin
   *     will be accepted.
   */
  constructor(methodList, serverOriginURLFilter) {
    /**
     * @private @const {!string} Filter to use to validate
     * the origin of received messages.  The origin of messages
     * must exactly match this value.
     */
    this.serverOriginURLFilter_ = serverOriginURLFilter;

    /**
     * The parent window.
     * @private {Window}
     */
    this.parentWindow_ = null;
    /*
     * @private {number}
     */
    this.nextMethodId_ = 0;
    /**
     * Map of methods awaiting a response.
     * @private {!Map}
     */
    this.methodsAwaitingResponse_ = new Map;
    /**
     * Function property that tracks whether client has
     * been initialized by the server.
     * @private {Function}
     */
    this.boundOnInitialize_ = null;

    // Generate the client-callable methods.
    this.generateAPIMethods_(methodList);
  }

  /*
   * Generates methods for the specified method names.
   * @private
   * @param {!Array<string>} methods  The names of the methods.
   */
  generateAPIMethods_(methods) {
    methods.forEach((method) => {
      Object.getPrototypeOf(this)[method] = function(args) {
        return this.callApiFn_(method, args);
      };
    });
  }

  initialize() {
    this.boundOnInitialize_ = this.onInitialize_.bind(this);
    // Wait for an init message from the server.
    window.addEventListener('message', this.boundOnInitialize_);
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
    if (!this.originMatchesFilter_(event.origin)) {
      console.error(
          'Initialization event received from non-authorized origin: ' +
          event.origin);
      return;
    }
    this.parentWindow_ = event.source;
    window.removeEventListener('message', this.boundOnInitialize_);
    this.boundOnInitialize_ = null;
    window.addEventListener('message', this.onMessage_.bind(this));
  }

  /**
   * Determine if the specified server origin URL matches the origin filter.
   * @param {!string} origin The origin URL to match with the filter.
   * @return {boolean}  whether the specified origin matches the filter.
   */
  originMatchesFilter_(origin) {
    return origin == this.serverOriginURLFilter_;
  }

  /**
   * Handles postMessage events sent from the server.
   * @param {Event} event  An event received from the server via the postMessage
   *     API.
   */
  onMessage_(event) {
    if (!this.originMatchesFilter_(event.origin)) {
      console.error(
          'Message received from non-authorized origin: ' + event.origin);
      return;
    }
    if (event.source != this.parentWindow_) {
      console.error('discarding event whose source is not the parent window');
      return;
    }
    if (!this.methodsAwaitingResponse_.has(event.data.methodId)) {
      console.error('discarding event method is not waiting for a response');
      return;
    }
    let method = this.methodsAwaitingResponse_.get(event.data.methodId);
    this.methodsAwaitingResponse_.delete(event.data.methodId);
    method(event.data.result);
  }

  /**
   * Converts a function call with arguments into a postMessage event
   * and sends it to the server via the postMessage API.
   * @param {string} fn  The function to call.
   * @param {!Array<Object>} args The arguments to pass to the function.
   * @return {!Promise} A promise capturing the executing of the function.
   */
  callApiFn_(fn, args) {
    let newMethodId = this.nextMethodId_++;
    let promise = new Promise((resolve, reject) => {
      if (!this.parentWindow_) {
        reject('No parent window defined');
      }
      this.parentWindow_.postMessage(
          {
            methodId: newMethodId,
            fn: fn,
            args: args,
          },
          '*');

      this.methodsAwaitingResponse_.set(newMethodId, resolve);
    });
    return promise;
  }
}
