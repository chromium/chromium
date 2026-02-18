// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './reload_button.js';
import './split_tabs_button.js';

import {TrackedElementManager} from '//resources/js/tracked_element/tracked_element_manager.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import type {LayoutConstants} from './browser_controls_api_data_model.mojom-webui.js';
import {type BrowserProxy, BrowserProxyImpl} from './browser_proxy.js';
import {MetricsRecorder} from './metrics_recorder.js';

export class ToolbarAppElement extends CrLitElement {
  static get is() {
    return 'toolbar-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      isReloadButtonEnabled_: {type: Boolean},
      isSplitTabsButtonEnabled_: {type: Boolean},
    };
  }

  protected accessor isReloadButtonEnabled_: boolean =
      loadTimeData.getBoolean('enableReloadButton');
  protected accessor isSplitTabsButtonEnabled_: boolean =
      loadTimeData.getBoolean('enableSplitTabsButton');

  private browserProxy_: BrowserProxy;
  private metricsRecorder_: MetricsRecorder;
  private trackedElementManager_: TrackedElementManager;
  private listenerIds_: number[] = [];

  constructor() {
    super();
    this.browserProxy_ = BrowserProxyImpl.getInstance();
    this.metricsRecorder_ = new MetricsRecorder(this.browserProxy_);
    this.trackedElementManager_ = TrackedElementManager.getInstance();
    ColorChangeUpdater.forDocument().start();
  }

  /**
   * Sets up event listeners and the PerformanceObserver when the element is
   * added to the DOM.
   */
  override connectedCallback() {
    super.connectedCallback();

    // Initial setup of CSS variables
    const gap = loadTimeData.getInteger('toolbarIconDefaultMargin');
    this.style.setProperty('--toolbar-icon-default-margin', `${gap}px`);
    this.style.setProperty(
        '--toolbar-button-height',
        `${loadTimeData.getInteger('toolbarButtonHeight')}px`);
    this.style.setProperty(
        '--toolbar-button-icon-size',
        `${loadTimeData.getInteger('toolbarButtonIconSize')}px`);
    this.style.setProperty(
        '--split-tabs-indicator-width',
        `${loadTimeData.getInteger('splitTabsIndicatorWidth')}px`);
    this.style.setProperty(
        '--split-tabs-indicator-height',
        `${loadTimeData.getInteger('splitTabsIndicatorHeight')}px`);
    this.style.setProperty(
        '--split-tabs-indicator-spacing',
        `${loadTimeData.getInteger('splitTabsIndicatorSpacing')}px`);

    this.listenerIds_.push(
        this.browserProxy_.callbackRouter.onLayoutChanged.addListener(
            (layoutConstants: LayoutConstants) =>
                this.onLayoutChanged_(layoutConstants)));

    this.browserProxy_.handler.getLayoutConstants().then(
        ({layoutConstants}) => {
          this.onLayoutChanged_(layoutConstants);
        });

    this.metricsRecorder_.startObserving();
    const reload = this.shadowRoot.querySelector<CrLitElement>('#reload');
    if (reload) {
      this.trackedElementManager_.startTracking(
          reload, 'kReloadButtonElementId');
    }
    const splitTabs =
        this.shadowRoot.querySelector<CrLitElement>('#split-tabs');
    if (splitTabs) {
      this.trackedElementManager_.startTracking(
          splitTabs, 'kToolbarSplitTabsToolbarButtonElementId');
    }
  }

  /**
   * Cleans up event listeners and the PerformanceObserver when the element is
   * removed from the DOM.
   */
  override disconnectedCallback() {
    super.disconnectedCallback();

    this.listenerIds_.forEach(
        id => this.browserProxy_.callbackRouter.removeListener(id));
    this.listenerIds_ = [];

    this.metricsRecorder_.stopObserving();
    const reload = this.shadowRoot.querySelector<HTMLElement>('#reload');
    if (reload) {
      this.trackedElementManager_.stopTracking(reload);
    }
  }

  private onLayoutChanged_(constants: LayoutConstants) {
    this.style.setProperty(
        '--toolbar-button-height', `${constants.toolbarButtonHeight}px`);
    this.style.setProperty(
        '--toolbar-button-icon-size', `${constants.toolbarButtonIconSize}px`);
  }

  override firstUpdated(changedProperties: PropertyValues<this>) {
    super.firstUpdated(changedProperties);
    const promises = [];
    const reload = this.shadowRoot.querySelector<CrLitElement>('#reload');
    if (reload) {
      promises.push(reload.updateComplete);
    }
    const splitTabs =
        this.shadowRoot.querySelector<CrLitElement>('#split-tabs');
    if (splitTabs) {
      promises.push(splitTabs.updateComplete);
    }
    Promise.all(promises).then(() => {
      this.browserProxy_.handler.onPageInitialized();
    });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'toolbar-app': ToolbarAppElement;
  }
}

customElements.define(ToolbarAppElement.is, ToolbarAppElement);
