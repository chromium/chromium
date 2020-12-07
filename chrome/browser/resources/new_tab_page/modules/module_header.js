// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';
import {html, microTask, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxy} from '../browser_proxy.js';

/** @fileoverview Element that displays a header inside a module. */

class ModuleHeaderElement extends PolymerElement {
  static get is() {
    return 'ntp-module-header';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * The ID of the module.
       * @type {!string}
       */
      moduleId: String,

      /**
       * The title to be displayed.
       * @type {!string}
       */
      title: String,

      /**
       * True if the header should display an info button.
       * @type {boolean}
       */
      showInfoButton: {
        type: Boolean,
        value: false,
      },

      /**
       * True if the header should display a dismiss button.
       * @type {boolean}
       */
      showDismissButton: {
        type: Boolean,
        value: false,
      },
    };
  }

  ready() {
    super.ready();
    const observer = new IntersectionObserver(([{intersectionRatio}]) => {
      if (intersectionRatio >= .5) {
        observer.disconnect();
        BrowserProxy.getInstance().handler.onModuleImpression(
            this.moduleId, BrowserProxy.getInstance().now());
      }
    }, {threshold: .5});
    // Calling observe will immediately invoke the callback. If the header is
    // fully shown when the page loads, the first callback invocation will
    // happen before the header has dimensions. For this reason, we start
    // observing after the element has had a chance to be rendered.
    microTask.run(() => {
      observer.observe(this);
    });
  }

  /** @private */
  onInfoButtonClick_() {
    this.dispatchEvent(new CustomEvent('info-button-click', {bubbles: true}));
  }

  /** @private */
  onDismissButtonClick_() {
    this.dispatchEvent(
        new CustomEvent('dismiss-button-click', {bubbles: true}));
  }
}

customElements.define(ModuleHeaderElement.is, ModuleHeaderElement);
