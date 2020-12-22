// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';
import {html, microTask, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxy} from '../browser_proxy.js';
import {ModuleDescriptor} from './module_descriptor.js';

/** @fileoverview Element that implements the common module UI. */

class ModuleWrapperElement extends PolymerElement {
  static get is() {
    return 'ntp-module-wrapper';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {!ModuleDescriptor} */
      descriptor: {
        observer: 'onDescriptorChange_',
        type: Object,
      },
    };
  }

  /**
   * @param {ModuleDescriptor} newValue
   * @param {ModuleDescriptor} oldValue
   * @private
   */
  onDescriptorChange_(newValue, oldValue) {
    assert(!oldValue);
    this.$.moduleElement.appendChild(this.descriptor.element);
    this.$.moduleElement.style.height = `${this.descriptor.heightPx}px`;

    // Log at most one usage per module per NTP page load. This is possible,
    // if a user opens a link in a new tab.
    this.descriptor.element.addEventListener('usage', () => {
      BrowserProxy.getInstance().handler.onModuleUsage(this.descriptor.id);
    }, {once: true});

    // Install observer to log module impression.
    const observer = new IntersectionObserver(([{intersectionRatio}]) => {
      if (intersectionRatio >= 1.0) {
        observer.disconnect();
        BrowserProxy.getInstance().handler.onModuleImpression(
            this.descriptor.id, BrowserProxy.getInstance().now());
      }
    }, {threshold: 1.0});
    // Calling observe will immediately invoke the callback. If the module is
    // fully shown when the page loads, the first callback invocation will
    // happen before the impression probe has dimensions. For this reason, we
    // start observing after the element has had a chance to be rendered.
    microTask.run(() => {
      observer.observe(this.$.impressionProbe);
    });
  }
}

customElements.define(ModuleWrapperElement.is, ModuleWrapperElement);
