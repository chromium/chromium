// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement, render} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {recordLoadDuration, recordOccurence, recordPerdecage} from '../metrics_utils.js';
import {NewTabPageProxy} from '../new_tab_page_proxy.js';
import {WindowProxy} from '../window_proxy.js';

import type {ModuleDescriptor} from './module_descriptor.js';
import {getCss} from './module_wrapper.css.js';
import {getHtml} from './module_wrapper.html.js';

/** @fileoverview Element that implements the common module UI. */

export interface ModuleInstance {
  element: HTMLElement;
  descriptor: ModuleDescriptor;
  initialized: boolean;
  impressed: boolean;
}

export interface ModuleWrapperElement {
  $: {
    moduleElement: HTMLElement,
    impressionProbe: HTMLElement,
  };
}

export class ModuleWrapperElement extends CrLitElement {
  static get is() {
    return 'ntp-module-wrapper';
  }

  static override get styles() {
    return getCss();
  }

  static override get properties() {
    return {
      module: {
        type: Object,
      },
    };
  }

  accessor module: ModuleInstance;

  override render() {
    // Update the light DOM element(s) and allow Lit to handle the shadow DOM
    // with a slotted module's UI element.
    if (this.module) {
      render(this.module.element, this, {host: this});
    }
    return getHtml.bind(this)();
  }

  override firstUpdated() {
    if (!this.module.initialized) {
      this.module.initialized = true;
      this.initModuleInstance_();
    }

    if (!this.module.impressed) {
      // Install observer to log module header impression.
      const headerObserver =
          new IntersectionObserver(([{intersectionRatio}]) => {
            if (intersectionRatio >= 1.0) {
              headerObserver.disconnect();

              const time = WindowProxy.getInstance().now();
              recordLoadDuration('NewTabPage.Modules.Impression', time);
              recordLoadDuration(
                  `NewTabPage.Modules.Impression.${this.module.descriptor.id}`,
                  time);
              this.module.impressed = true;
              this.dispatchEvent(new Event('detect-impression'));
              this.module.element.dispatchEvent(new Event('detect-impression'));
            }
          }, {threshold: 1.0});
      headerObserver.observe(this.$.impressionProbe);
    }
  }

  private initModuleInstance_() {
    // Log at most one usage per module per NTP page load. This is possible,
    // if a user opens a link in a new tab.
    this.module.element.addEventListener('usage', (e: Event) => {
      e.stopPropagation();
      NewTabPageProxy.getInstance().handler.onModuleUsed(
          this.module.descriptor.id);

      recordOccurence('NewTabPage.Modules.Usage');
      recordOccurence(`NewTabPage.Modules.Usage.${this.module.descriptor.id}`);
    }, {once: true});

    // Dispatch at most one interaction event for a module's `More Actions` menu
    // button clicks.
    this.module.element.addEventListener('menu-button-click', (e: Event) => {
      e.stopPropagation();
      NewTabPageProxy.getInstance().handler.onModuleUsed(
          this.module.descriptor.id);
    }, {once: true});

    // Log module's id when module's info button is clicked.
    this.module.element.addEventListener('info-button-click', () => {
      chrome.metricsPrivate.recordSparseValueWithPersistentHash(
          'NewTabPage.Modules.InfoButtonClicked', this.module.descriptor.id);
    }, {once: true});

    // Track whether the user hovered on the module.
    this.module.element.addEventListener('mouseover', () => {
      chrome.metricsPrivate.recordSparseValueWithPersistentHash(
          'NewTabPage.Modules.Hover', this.module.descriptor.id);
    }, {
      capture: true,  // So that modules cannot swallow event.
      once: true,     // Only one log per NTP load.
    });

    // Install observer to track max perdecage (x/10th) of the module visible
    // on the page.
    let intersectionPerdecage = 0;
    const moduleObserver = new IntersectionObserver(([{intersectionRatio}]) => {
      intersectionPerdecage =
          Math.floor(Math.max(intersectionPerdecage, intersectionRatio * 10));
    }, {threshold: [0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1]});
    // Use `pagehide` rather than `unload` because unload is being deprecated.
    // `pagehide` fires with the same timing and is safe to use since NTP never
    // enters back/forward-cache.
    window.addEventListener('pagehide', () => {
      recordPerdecage(
          'NewTabPage.Modules.ImpressionRatio', intersectionPerdecage);
      recordPerdecage(
          `NewTabPage.Modules.ImpressionRatio.${this.module.descriptor.id}`,
          intersectionPerdecage);
    });
    moduleObserver.observe(this);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ntp-module-wrapper': ModuleWrapperElement;
  }
}

customElements.define(ModuleWrapperElement.is, ModuleWrapperElement);
