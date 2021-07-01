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
import {html, PolymerElement, TemplateInstanceBase, templatize} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ensureLazyLoaded} from '../ensure_lazy_loaded.js';

/** @polymer */
class SettingsIdleLoadElement extends PolymerElement {
  static get is() {
    return 'settings-idle-load';
  }

  static get template() {
    return html`<slot></slot>`;
  }

  static get properties() {
    return {
      /**
       * If specified, it will be loaded via an HTML import before stamping the
       * template.
       */
      url: String,
    };
  }

  constructor() {
    super();

    /** @private {?Element} */
    this.child_ = null;

    /** @private {?Element|?TemplateInstanceBase} */
    this.instance_ = null;

    /** @private {number} */
    this.idleCallback_ = 0;

    /** @private {?Promise<!Element>} */
    this.loading_ = null;
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();

    this.idleCallback_ = requestIdleCallback(() => {
      this.get();
    });
  }

  /** @override */
  disconnectedCallback() {
    super.disconnectedCallback();

    // No-op if callback already fired.
    cancelIdleCallback(this.idleCallback_);
  }

  /**
   * @return {!Promise<!Element>} Resolves with the stamped child element after
   *     the lazy module has been loaded.
   */
  requestLazyModule_() {
    return new Promise((resolve, reject) => {
      ensureLazyLoaded().then(() => {
        const template = /** @type {!HTMLTemplateElement} */ (
            this.shadowRoot.querySelector('slot')
                .assignedNodes({flatten: true})
                .filter(n => n.nodeType === Node.ELEMENT_NODE)[0]);

        const TemplateClass = templatize(template, this, {
          mutableData: false,
          forwardHostProp: this._forwardHostPropV2,
        });

        this.instance_ = new TemplateClass();

        assert(!this.child_);
        this.child_ = this.instance_.root.firstElementChild;

        this.parentNode.insertBefore(this.instance_.root, this);
        resolve(this.child_);

        this.dispatchEvent(
            new CustomEvent('lazy-loaded', {bubbles: true, composed: true}));
      }, reject);
    });
  }

  /**
   * @return {!Promise<!Element>} Child element which has been stamped into the
   *     DOM tree.
   */
  get() {
    if (this.loading_) {
      return this.loading_;
    }

    this.loading_ = this.requestLazyModule_();
    return this.loading_;
  }

  /**
   * @param {string} prop
   * @param {Object} value
   */
  _forwardHostPropV2(prop, value) {
    if (this.instance_) {
      this.instance_.forwardHostProp(prop, value);
    }
  }
}

customElements.define(SettingsIdleLoadElement.is, SettingsIdleLoadElement);
