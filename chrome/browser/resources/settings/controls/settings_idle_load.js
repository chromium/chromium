// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * settings-idle-load is a simple variant of dom-if designed for lazy
 * loading and rendering of elements that are accessed imperatively. A URL is
 * given that holds the elements to be loaded lazily.
 */
Polymer({
  is: 'settings-idle-load',

  behaviors: [Polymer.Templatizer],

  properties: {
    /**
     * If specified, it will be loaded via an HTML import before stamping the
     * template.
     */
    url: String,
  },

  /** @private {?Element} */
  child_: null,

  /** @private {?Element} */
  instance_: null,

  /** @private {number} */
  idleCallback_: 0,

  /** @override */
  attached: function() {
    this.idleCallback_ = requestIdleCallback(this.get.bind(this));
  },

  /** @override */
  detached: function() {
    // No-op if callback already fired.
    cancelIdleCallback(this.idleCallback_);
  },

  /**
   * @return {!Promise<Element>} Child element which has been stamped into the
   *     DOM tree.
   */
  get: function() {
    if (this.loading_) {
      return this.loading_;
    }

    this.loading_ = new Promise((resolve, reject) => {
      this.importHref(this.url, () => {
        assert(!this.ctor);
        this.templatize(this.getContentChildren()[0]);
        assert(this.ctor);

        this.instance_ = this.stamp({});

        assert(!this.child_);
        this.child_ = this.instance_.root.firstElementChild;

        this.parentNode.insertBefore(this.instance_.root, this);
        resolve(this.child_);

        this.fire('lazy-loaded');
      }, reject, true);
    });

    return this.loading_;
  },

  /**
   * @param {string} prop
   * @param {Object} value
   */
  _forwardHostPropV2: function(prop, value) {
    if (this.instance_) {
      this.instance_.forwardHostProp(prop, value);
    }
  },
});
