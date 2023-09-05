// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Handler for requests that come to the window containing the contents.
export class RequestHandler {
  constructor(clientElement, messageOriginURLFilter, targetURL) {
    /**
     * Map that stores references to the methods implemented by the API.
     * @private {!Map<string, function(!Array):?>}
     */
    this.apiFns_ = new Map();

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

    window.addEventListener('message', (event) => {
      this.handleMessage_(event);
    });
  }

  /**
   * Returns the target url that this request handler is communicating with.
   * @return {URL}
   */
  targetUrl() {
    return this.targetURL_;
  }

  /**
   * Returns the target window that this request handler is communicating with.
   * @return {!Window}
   */
  targetWindow() {
    return this.clientElement().contentWindow;
  }

  /**
   * The Window type element to which this request handler will listen for
   * messages.
   * @return {!Element}
   */
  clientElement() {
    return this.clientElement_;
  }

  /**
   * Determines if the specified origin matches the origin filter.
   * @param {!string} origin The origin URL to match with the filter.
   * @return {boolean}  whether the specified origin matches the filter.
   */
  originMatchesFilter(origin) {
    const originURL = new URL(origin);

    // We allow the pathname portion of the URL to be a prefix filter,
    // to permit for different paths communicating with this server.
    return originURL.protocol === this.messageOriginURLFilter_.protocol &&
        originURL.host === this.messageOriginURLFilter_.host &&
        originURL.pathname.startsWith(this.messageOriginURLFilter_.pathname);
  }

  /**
   * Registers the specified method name with the specified
   * function.
   *
   * @param {!string} methodName name of the method to register.
   * @param {!function(!Array):?} method The function to associate with the
   *     name.
   */
  registerMethod(methodName, method) {
    this.apiFns_.set(methodName, method);
  }

  /**
   * Handles postMessage events from the client.
   * @private
   * @param {Event} event  The postMessage event to handle.
   */
  async handleMessage_(event) {
    if (!this.originMatchesFilter(event.origin)) {
      console.info(
          'Message received from unauthorized origin: ' + event.origin);
      return;
    }

    // If we have gotten this far, we have received a message from a trusted
    // origin, and we should try to process it.  We can't gate this on whether
    // the channel is initialized, because we can receive events out of order,
    // and method calls can be received before the init event. Essentially, we
    // should treat the channel as being potentially as soon as we send 'init'
    // to the guest content.
    const methodId = event.data.methodId;
    const fn = event.data.fn;
    const args = event.data.args || [];

    if (!this.canHandle(fn)) {
      console.info('Unknown function requested: ' + fn);
      return;
    }

    const sendMessage = (methodId, result, rejected, error) => {
      this.targetWindow().postMessage(
          {
            methodId: methodId,
            result: result,
            rejected: rejected,
            error: error,
          },
          this.targetUrl().toString());
    };


    try {
      sendMessage(methodId, await this.handle(fn, args), false, null);
    } catch (error) {
      sendMessage(methodId, null, true, error);
    }
  }
  /**
   * Executes the method and returns the result.
   *
   * @param {!string} funcName name of method to be executed.
   * @param {!Array} args the arguments to the method being executed.
   * @return {Promise<!Object>} returns a promise of the object returned.
   */
  handle(funcName, args) {
    if (!this.apiFns_.has(funcName)) {
      return Promise.reject('Unknown function requested' + funcName);
    }

    return this.apiFns_.get(funcName)(args);
  }

  /**
   * Check whether the method can be handled by this handler.
   * @param{!string} funcName name of method
   * @return {boolean}
   */
  canHandle(funcName) {
    return this.apiFns_.has(funcName);
  }
}
