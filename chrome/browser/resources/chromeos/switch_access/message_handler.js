// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Handles sending messages between the background page and the context menu.
 */
const MessageHandler = {
  /**
   * function setActions() takes a list of comma-separated action names,
   * containing only letters and hyphens.
   */
  SET_ACTIONS_REGEX: /setActions[(]((?:[a-zA-Z-]+,)*[a-zA-Z-]+)[)]/,

  /**
   * function setFocusRing() takes one element ID, containing only letters,
   * hyphens, and underscores, and either 'on' or 'off'.
   */
  SET_FOCUS_RING_REGEX: /setFocusRing[(]([a-zA-Z_-]+),((?:on)|(?:off))[)]/,

  /**
   * The possible destinations for messages.
   * @enum {string}
   * @const
   */
  Destination: {BACKGROUND: 'background', PANEL: 'panel.html'},

  /**
   * Sends a message to the Context Menu panel with the given name and
   * parameters.
   *
   * @param {!MessageHandler.Destination} destination
   * @param {string} messageName
   * @param {Array<string>=} opt_params
   */
  sendMessage: (destination, messageName, opt_params) => {
    let message = messageName;
    if (opt_params)
      message += '(' + opt_params.join(',') + ')';

    const views = chrome.extension.getViews() || [];
    for (const view of views)
      if (view && view.location.href.includes(destination)) {
        view.postMessage(message, window.location.origin);
        return;
      }
    console.log('Couldn\'t find ', destination);
  }
};
