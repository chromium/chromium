// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * -- WebviewScrollShadowsHelper --
 *
 *  Sends scroll information from within the webview. This information is used
 *  to appropriately add classes to the webview in order to display shadows on
 *  top of it. The shadows show to the user if there is content hidden that
 *  could be seen if the user would scroll up/down.
 */

export const WebviewScrollShadowsHelper = (function() {
  function WebviewScrollShadowsHelper() {}

  WebviewScrollShadowsHelper.prototype = {
    init(channel) {
      this.channel_ = channel;

      window.addEventListener('scroll', this.sendScrollInfo_.bind(this));
      window.addEventListener('resize', this.sendScrollInfo_.bind(this));
      this.boundAttachResizeObserver_ = this.attachResizeObserver_.bind(this);
      window.addEventListener('load', this.boundAttachResizeObserver_);
      this.resizeObserver = new ResizeObserver(() => {
        this.sendScrollInfo_();
      });
    },

    // Observe when document.body changes in size.
    attachResizeObserver_(event) {
      this.resizeObserver.observe(document.body);
      window.removeEventListener(event.type, this.boundAttachResizeObserver_);
    },

    sendScrollInfo_(event) {
      this.channel_.send({
        name: 'scrollInfo',
        scrollTop: window.scrollY,
        scrollHeight: document.body.scrollHeight,
      });
    },
  };

  return WebviewScrollShadowsHelper;
})();

export const WebviewScrollShadowsHelperConstructor = function() {
  return new WebviewScrollShadowsHelper();
};
