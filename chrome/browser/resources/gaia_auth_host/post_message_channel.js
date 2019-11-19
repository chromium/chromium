// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Provides a HTML5 postMessage channel to the injected JS to talk back
 * to Authenticator.
 */
'use strict';

// <include src="channel.js">

const PostMessageChannel = (function() {
  /**
   * Allowed origins of the hosting page.
   * @type {Array<string>}
   */
  const ALLOWED_ORIGINS =
      ['chrome://oobe', 'chrome://chrome-signin', 'chrome://password-change'];

  /** @const */
  const PORT_MESSAGE = 'post-message-port-message';

  /** @const */
  const CHANNEL_INIT_MESSAGE = 'post-message-channel-init';

  /** @const */
  const CHANNEL_CONNECT_MESSAGE = 'post-message-channel-connect';

  /**
   * Whether the script runs in a top level window.
   */
  function isTopLevel() {
    return window === window.top;
  }

  /**
   * A simple event target.
   */
  function EventTarget() {
    this.listeners_ = [];
  }

  EventTarget.prototype = {
    /**
     * Add an event listener.
     */
    addListener: function(listener) {
      this.listeners_.push(listener);
    },

    /**
     * Dispatches a given event to all listeners.
     */
    dispatch: function(e) {
      for (let i = 0; i < this.listeners_.length; ++i) {
        this.listeners_[i].call(undefined, e);
      }
    }
  };

  /**
   * ChannelManager handles window message events by dispatching them to
   * PostMessagePorts or forwarding to other windows (up/down the hierarchy).
   * @constructor
   */
  function ChannelManager() {
    /**
     * Window and origin to forward message up the hierarchy. For subframes,
     * they defaults to window.parent and any origin. For top level window,
     * this would be set to the hosting webview on CHANNEL_INIT_MESSAGE.
     */
    this.upperWindow = isTopLevel() ? null : window.parent;
    this.upperOrigin = isTopLevel() ? '' : '*';

    /**
     * Channle Id to port map.
     * @type {Object<number, PostMessagePort>}
     */
    this.channels_ = {};

    /**
     * Deferred messages to be posted to |upperWindow|.
     * @type {Array}
     */
    this.deferredUpperWindowMessages_ = [];

    /**
     * Ports that depend on upperWindow and need to be setup when its available.
     */
    this.deferredUpperWindowPorts_ = [];

    /**
     * Whether the ChannelManager runs in daemon mode and accepts connections.
     */
    this.isDaemon = false;

    /**
     * Fires when ChannelManager is in listening mode and a
     * CHANNEL_CONNECT_MESSAGE is received.
     */
    this.onConnect = new EventTarget();

    window.addEventListener('message', this.onMessage_.bind(this));
  }

  ChannelManager.prototype = {
    /**
     * Gets a global unique id to use.
     * @return {number}
     */
    createChannelId_: function() {
      return (new Date()).getTime();
    },

    /**
     * Posts data to upperWindow. Queue it if upperWindow is not available.
     */
    postToUpperWindow: function(data) {
      if (this.upperWindow == null) {
        this.deferredUpperWindowMessages_.push(data);
        return;
      }

      this.upperWindow.postMessage(data, this.upperOrigin);
    },

    /**
     * Creates a port and register it in |channels_|.
     * @param {number} channelId
     * @param {string} channelName
     * @param {DOMWindow=} opt_targetWindow
     * @param {string=} opt_targetOrigin
     */
    createPort: function(
        channelId, channelName, opt_targetWindow, opt_targetOrigin) {
      const port = new PostMessagePort(channelId, channelName);
      if (opt_targetWindow) {
        port.setTarget(opt_targetWindow, opt_targetOrigin);
      }
      this.channels_[channelId] = port;
      return port;
    },

    /*
     * Returns a message forward handler for the given proxy port.
     * @private
     */
    getProxyPortForwardHandler_: function(proxyPort) {
      return function(msg) {
        proxyPort.postMessage(msg);
      };
    },

    /**
     * Creates a forwarding porxy port.
     * @param {number} channelId
     * @param {string} channelName
     * @param {!DOMWindow} targetWindow
     * @param {!string} targetOrigin
     */
    createProxyPort: function(
        channelId, channelName, targetWindow, targetOrigin) {
      const port =
          this.createPort(channelId, channelName, targetWindow, targetOrigin);
      port.onMessage.addListener(this.getProxyPortForwardHandler_(port));
      return port;
    },

    /**
     * Creates a connecting port to the daemon and request connection.
     * @param {string} name
     * @return {PostMessagePort}
     */
    connectToDaemon: function(name) {
      if (this.isDaemon) {
        console.error(
            'Error: Connecting from the daemon page is not supported.');
        return;
      }

      const port = this.createPort(this.createChannelId_(), name);
      if (this.upperWindow) {
        port.setTarget(this.upperWindow, this.upperOrigin);
      } else {
        this.deferredUpperWindowPorts_.push(port);
      }

      this.postToUpperWindow({
        type: CHANNEL_CONNECT_MESSAGE,
        channelId: port.channelId,
        channelName: port.name
      });
      return port;
    },

    /**
     * Dispatches a 'message' event to port.
     * @private
     */
    dispatchMessageToPort_: function(e) {
      const channelId = e.data.channelId;
      const port = this.channels_[channelId];
      if (!port) {
        console.error('Error: Unable to dispatch message. Unknown channel.');
        return;
      }

      port.handleWindowMessage(e);
    },

    /**
     * Window 'message' handler.
     */
    onMessage_: function(e) {
      if (typeof e.data != 'object' || !e.data.hasOwnProperty('type')) {
        return;
      }

      if (e.data.type === PORT_MESSAGE) {
        // Dispatch port message to ports if this is the daemon page or
        // the message is from upperWindow. In case of null upperWindow,
        // the message is assumed to be forwarded to upperWindow and queued.
        if (this.isDaemon ||
            (this.upperWindow && e.source === this.upperWindow)) {
          this.dispatchMessageToPort_(e);
        } else {
          this.postToUpperWindow(e.data);
        }
      } else if (e.data.type === CHANNEL_CONNECT_MESSAGE) {
        const channelId = e.data.channelId;
        const channelName = e.data.channelName;

        if (this.isDaemon) {
          const port =
              this.createPort(channelId, channelName, e.source, e.origin);
          this.onConnect.dispatch(port);
        } else {
          this.createProxyPort(channelId, channelName, e.source, e.origin);
          this.postToUpperWindow(e.data);
        }
      } else if (e.data.type === CHANNEL_INIT_MESSAGE) {
        if (ALLOWED_ORIGINS.indexOf(e.origin) == -1) {
          return;
        }

        this.upperWindow = e.source;
        this.upperOrigin = e.origin;

        for (let i = 0; i < this.deferredUpperWindowMessages_.length; ++i) {
          this.upperWindow.postMessage(
              this.deferredUpperWindowMessages_[i], this.upperOrigin);
        }
        this.deferredUpperWindowMessages_ = [];

        for (let i = 0; i < this.deferredUpperWindowPorts_.length; ++i) {
          this.deferredUpperWindowPorts_[i].setTarget(
              this.upperWindow, this.upperOrigin);
        }
        this.deferredUpperWindowPorts_ = [];
      }
    }
  };

  /**
   * Singleton instance of ChannelManager.
   * @type {ChannelManager}
   */
  const channelManager = new ChannelManager();

  /**
   * A HTML5 postMessage based port that provides the same port interface
   * as the messaging API port.
   * @param {number} channelId
   * @param {string} name
   */
  function PostMessagePort(channelId, name) {
    this.channelId = channelId;
    this.name = name;
    this.targetWindow = null;
    this.targetOrigin = '';
    this.deferredMessages_ = [];

    this.onMessage = new EventTarget();
  }

  PostMessagePort.prototype = {
    /**
     * Sets the target window and origin.
     * @param {DOMWindow} targetWindow
     * @param {string} targetOrigin
     */
    setTarget: function(targetWindow, targetOrigin) {
      this.targetWindow = targetWindow;
      this.targetOrigin = targetOrigin;

      for (let i = 0; i < this.deferredMessages_.length; ++i) {
        this.postMessage(this.deferredMessages_[i]);
      }
      this.deferredMessages_ = [];
    },

    postMessage: function(msg) {
      if (!this.targetWindow) {
        this.deferredMessages_.push(msg);
        return;
      }

      this.targetWindow.postMessage(
          {type: PORT_MESSAGE, channelId: this.channelId, payload: msg},
          this.targetOrigin);
    },

    handleWindowMessage: function(e) {
      this.onMessage.dispatch(e.data.payload);
    }
  };

  /**
   * A message channel based on PostMessagePort.
   * @extends {Channel}
   * @constructor
   */
  function PostMessageChannel() {
    Channel.apply(this, arguments);
  }

  PostMessageChannel.prototype = {
    __proto__: Channel.prototype,

    /** @override */
    connect: function(name) {
      this.port_ = channelManager.connectToDaemon(name);
      this.port_.onMessage.addListener(this.onMessage_.bind(this));
    },
  };

  /**
   * Initialize webview content window for postMessage channel.
   * @param {DOMWindow} webViewContentWindow Content window of the webview.
   */
  PostMessageChannel.init = function(webViewContentWindow) {
    webViewContentWindow.postMessage({type: CHANNEL_INIT_MESSAGE}, '*');
  };

  /**
   * Run in daemon mode and listen for incoming connections. Note that the
   * current implementation assumes the daemon runs in the hosting page
   * at the upper layer of the DOM tree. That is, all connect requests go
   * up the DOM tree instead of going into sub frames.
   * @param {function(PostMessagePort)} callback Invoked when a connection is
   *     made.
   */
  PostMessageChannel.runAsDaemon = function(callback) {
    channelManager.isDaemon = true;

    const onConnect = function(port) {
      callback(port);
    };
    channelManager.onConnect.addListener(onConnect);
  };

  return PostMessageChannel;
})();

/** @override */
Channel.create = function() {
  return new PostMessageChannel();
};
