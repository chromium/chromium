// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './reload_button.js';

import {TrackedElementManager} from '//resources/js/tracked_element/tracked_element_manager.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import {BrowserProxyImpl} from './browser_proxy.js';
import type {BrowserProxy} from './browser_proxy.js';
import {MetricsRecorder} from './metrics_recorder.js';

export interface ToolbarAppElement {
  $: {
    reload: CrLitElement,
  };
}

export class ToolbarAppElement extends CrLitElement {
  private browserProxy_: BrowserProxy;
  private metricsRecorder_: MetricsRecorder;
  private trackedElementManager_: TrackedElementManager;

  constructor() {
    super();
    this.browserProxy_ = BrowserProxyImpl.getInstance();
    this.metricsRecorder_ = new MetricsRecorder(this.browserProxy_);
    this.trackedElementManager_ = TrackedElementManager.getInstance();
    ColorChangeUpdater.forDocument().start();
  }

  static get is() {
    return 'toolbar-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  /**
   * Sets up event listeners and the PerformanceObserver when the element is
   * added to the DOM.
   */
  override connectedCallback() {
    super.connectedCallback();

    this.metricsRecorder_.startObserving();
    this.trackedElementManager_.startTracking(
        this.$.reload, 'kReloadButtonElementId');
  }

  /**
   * Cleans up event listeners and the PerformanceObserver when the element is
   * removed from the DOM.
   */
  override disconnectedCallback() {
    super.disconnectedCallback();

    this.metricsRecorder_.stopObserving();
  }

  override async firstUpdated(changedProperties: PropertyValues) {
    super.firstUpdated(changedProperties);
    await this.$.reload.updateComplete;
    this.browserProxy_.handler.onPageInitialized();
  }
}


declare global {
  interface HTMLElementTagNameMap {
    'toolbar-app': ToolbarAppElement;
  }
}

customElements.define(ToolbarAppElement.is, ToolbarAppElement);
