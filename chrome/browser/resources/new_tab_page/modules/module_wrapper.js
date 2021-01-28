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

    // Install observer to log module header impression.
    const headerObserver = new IntersectionObserver(([{intersectionRatio}]) => {
      if (intersectionRatio >= 1.0) {
        headerObserver.disconnect();
        BrowserProxy.getInstance().handler.onModuleImpression(
            this.descriptor.id, BrowserProxy.getInstance().now());
      }
    }, {threshold: 1.0});

    // Install observer to track max perdecage (x/10th) of the module visible on
    // the page.
    let intersectionPerdecage = 0;
    const moduleObserver = new IntersectionObserver(([{intersectionRatio}]) => {
      intersectionPerdecage =
          Math.floor(Math.max(intersectionPerdecage, intersectionRatio * 10));
    }, {threshold: [0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1]});
    window.addEventListener('unload', () => {
      const recordPerdecage = (metricName, value) => {
        chrome.metricsPrivate.recordValue(
            {
              metricName,
              type: chrome.metricsPrivate.MetricTypeType.HISTOGRAM_LINEAR,
              min: 1,       // Choose 1 if real min is 0.
              max: 11,      // Exclusive.
              buckets: 12,  // Numbers 0-10 and unused overflow bucket of 11.
            },
            value);
      };
      recordPerdecage(
          'NewTabPage.Modules.ImpressionRatio', intersectionPerdecage);
      recordPerdecage(
          `NewTabPage.Modules.ImpressionRatio.${this.descriptor.id}`,
          intersectionPerdecage);
    });

    // Calling observe will immediately invoke the callback. If the module is
    // fully shown when the page loads, the first callback invocation will
    // happen before the elements have dimensions. For this reason, we start
    // observing after the elements have had a chance to be rendered.
    microTask.run(() => {
      headerObserver.observe(this.$.impressionProbe);
      moduleObserver.observe(this);
    });

    // Track whether the user hovered on the module.
    this.addEventListener('mouseover', () => {
      chrome.metricsPrivate.recordSparseHashable(
          'NewTabPage.Modules.Hover', this.descriptor.id);
    }, {
      useCapture: true,  // So that modules cannot swallow event.
      once: true,        // Only one log per NTP load.
    });
  }
}

customElements.define(ModuleWrapperElement.is, ModuleWrapperElement);
