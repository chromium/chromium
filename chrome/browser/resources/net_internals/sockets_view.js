// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This view displays information on the state of all socket pools.
 *
 *   - Has a button to close idle sockets.
 *   - Has a button to flush socket pools.
 */
const SocketsView = (function() {
  'use strict';

  // We inherit from DivView.
  const superClass = DivView;

  /**
   * @constructor
   */
  function SocketsView() {
    assertFirstConstructorCall(SocketsView);

    // Call superclass's constructor.
    superClass.call(this, SocketsView.MAIN_BOX_ID);

    const closeIdleButton = $(SocketsView.CLOSE_IDLE_SOCKETS_BUTTON_ID);
    closeIdleButton.onclick = this.closeIdleSockets.bind(this);

    const flushSocketsButton = $(SocketsView.SOCKET_POOL_FLUSH_BUTTON_ID);
    flushSocketsButton.onclick = this.flushSocketPools.bind(this);
  }

  SocketsView.TAB_ID = 'tab-handle-sockets';
  SocketsView.TAB_NAME = 'Sockets';
  SocketsView.TAB_HASH = '#sockets';

  // IDs for special HTML elements in sockets_view.html
  SocketsView.MAIN_BOX_ID = 'sockets-view-tab-content';
  SocketsView.CLOSE_IDLE_SOCKETS_BUTTON_ID = 'sockets-view-close-idle-button';
  SocketsView.SOCKET_POOL_FLUSH_BUTTON_ID = 'sockets-view-flush-button';

  cr.addSingletonGetter(SocketsView);

  SocketsView.prototype = {
    // Inherit the superclass's methods.
    __proto__: superClass.prototype,

    closeIdleSockets() {
      BrowserBridge.getInstance().sendCloseIdleSockets();
    },

    flushSocketPools() {
      BrowserBridge.getInstance().sendFlushSocketPools();
    }
  };

  return SocketsView;
})();
