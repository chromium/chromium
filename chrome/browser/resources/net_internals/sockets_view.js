// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$} from 'chrome://resources/js/util.js';

import {BrowserBridge} from './browser_bridge.js';
import {DivView} from './view.js';

/** @type {?SocketsView} */
let instance = null;

/**
 * This view displays information on the state of all socket pools.
 *
 *   - Has a button to close idle sockets.
 *   - Has a button to flush socket pools.
 */
export class SocketsView extends DivView {
  constructor() {
    // Call superclass's constructor.
    super(SocketsView.MAIN_BOX_ID);

    const closeIdleButton = $(SocketsView.CLOSE_IDLE_SOCKETS_BUTTON_ID);
    closeIdleButton.onclick = this.closeIdleSockets.bind(this);

    const flushSocketsButton = $(SocketsView.SOCKET_POOL_FLUSH_BUTTON_ID);
    flushSocketsButton.onclick = this.flushSocketPools.bind(this);
  }

  closeIdleSockets() {
    BrowserBridge.getInstance().sendCloseIdleSockets();
  }

  flushSocketPools() {
    BrowserBridge.getInstance().sendFlushSocketPools();
  }

  static getInstance() {
    return instance || (instance = new SocketsView());
  }
}

SocketsView.TAB_ID = 'tab-handle-sockets';
SocketsView.TAB_NAME = 'Sockets';
SocketsView.TAB_HASH = '#sockets';

// IDs for special HTML elements in sockets_view.html
SocketsView.MAIN_BOX_ID = 'sockets-view-tab-content';
SocketsView.CLOSE_IDLE_SOCKETS_BUTTON_ID = 'sockets-view-close-idle-button';
SocketsView.SOCKET_POOL_FLUSH_BUTTON_ID = 'sockets-view-flush-button';
