// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * settings-idle-load is a simple variant of dom-if designed for lazy
 * loading and rendering of elements that are accessed imperatively. A URL is
 * given that holds the elements to be loaded lazily.
 */
import {assert} from '//resources/js/assert.m.js';
import {html, Polymer, TemplateInstanceBase, templatize} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ensureLazyLoaded} from '../ensure_lazy_loaded.js';

Polymer({
  is: 'settings-idle-load',

  _template: html`<slot></slot>`,

  properties: {
    /**
     * If specified, it will be loaded via an HTML import before stamping the
     * template.
     */
    url: String,
  },

  /** @private {?Element} */
  child_: null,

  /** @private {?Element|?TemplateInstanceBase} */
  instance_: null,

  /** @private {number} */
  idleCallback_: 0,

  /** @override */
  attached() {
    this.idleCallback_ = requestIdleCallback(() => {
      this.get();
    });
  },

  /** @override */
  detached() {
    // No-op if callback already fired.
    cancelIdleCallback(this.idleCallback_);
  },

  /**
   * @return {!Promise<!Element>} Resolves with the stamped child element after
   *     the lazy module has been loaded.
   */
  requestLazyModule_() {
    return new Promise((resolve, reject) => {
      ensureLazyLoaded().then(() => {
        const template =
            /** @type {!HTMLTemplateElement} */ (this.getContentChildren()[0]);
        const TemplateClass = templatize(template, this, {
          mutableData: false,
          forwardHostProp: this._forwardHostPropV2,
        });

        this.instance_ = new TemplateClass();

        assert(!this.child_);
        this.child_ = this.instance_.root.firstElementChild;

        this.parentNode.insertBefore(this.instance_.root, this);
        resolve(this.child_);

        this.fire('lazy-loaded');
      }, reject);
    });
  },

  /**
   * @return {!Promise<Element>} Child element which has been stamped into the
   *     DOM tree.
   */
  get() {
    if (this.loading_) {
      return this.loading_;
    }

    this.loading_ = this.requestLazyModule_();
    return this.loading_;
  },

  /**
   * @param {string} prop
   * @param {Object} value
   */
  _forwardHostPropV2(prop, value) {
    if (this.instance_) {
      this.instance_.forwardHostProp(prop, value);
    }
  },
});
