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
import {BrowserProxyImpl, INVALID_NAVIGATION_CONTROLS_STATE_LISTENER_HANDLE} from './browser_proxy.js';
import type {BrowserProxy, NavigationControlsState, NavigationControlsStateListenerHandle} from './browser_proxy.js';
import {MetricsRecorder} from './metrics_recorder.js';
import {SplitTabActiveLocation} from './toolbar_ui_api_data_model.mojom-webui.js';

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
      isLocationBarEnabled_: {type: Boolean},
      navigationControlsState_: {type: Object},
    };
  }

  protected accessor isReloadButtonEnabled_: boolean =
      loadTimeData.getBoolean('enableReloadButton');
  protected accessor isSplitTabsButtonEnabled_: boolean =
      loadTimeData.getBoolean('enableSplitTabsButton');
  protected accessor isLocationBarEnabled_: boolean =
      loadTimeData.getBoolean('enableLocationBar');
  protected accessor navigationControlsState_: NavigationControlsState = {
    reloadControlState: {
      isDevtoolsConnected: false,
      isNavigationLoading: false,
      isContextMenuVisible: false,
    },
    splitTabsControlState: {
      isCurrentTabSplit: false,
      location: SplitTabActiveLocation.kStart,
      isPinned: false,
      isContextMenuVisible: false,
    },
    layoutConstantsVersion: 0,
  };

  private browserProxy_: BrowserProxy;
  private metricsRecorder_: MetricsRecorder;
  private trackedElementManager_: TrackedElementManager;
  private navigationStateListenerHandle_:
      NavigationControlsStateListenerHandle =
          INVALID_NAVIGATION_CONTROLS_STATE_LISTENER_HANDLE;

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
    this.style.setProperty(
        '--split-tabs-indicator-width',
        `${loadTimeData.getInteger('splitTabsIndicatorWidth')}px`);
    this.style.setProperty(
        '--split-tabs-indicator-height',
        `${loadTimeData.getInteger('splitTabsIndicatorHeight')}px`);
    this.style.setProperty(
        '--split-tabs-indicator-spacing',
        `${loadTimeData.getInteger('splitTabsIndicatorSpacing')}px`);

    this.setFontVariables('omniboxPrimary');

    this.navigationStateListenerHandle_ =
        this.browserProxy_.addNavigationStateListener(
            (state: NavigationControlsState) => {
              this.navigationControlsState_ = state;
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

  /* Sets CSS font variables for given prefix based on loadData.
   * See WebUIToolbarUI::AddFontVariables */
  setFontVariables(fontPrefix: string) {
    // from print_preview_sidebar_test.ts:
    function camelToKebab(s: string): string {
      return s.replace(/([A-Z])/g, '-$1').toLowerCase();
    }

    const cssVarPrefix = '--' + camelToKebab(fontPrefix);
    this.style.setProperty(
        `${cssVarPrefix}-font-family`,
        CSS.escape(loadTimeData.getString(`${fontPrefix}Family`)));
    this.style.setProperty(
        `${cssVarPrefix}-font-size`,
        loadTimeData.getInteger(`${fontPrefix}Size`) + 'px');
    this.style.setProperty(
        `${cssVarPrefix}-font-weight`,
        String(loadTimeData.getInteger(`${fontPrefix}Weight`)));
  }

  /**
   * Cleans up event listeners and the PerformanceObserver when the element is
   * removed from the DOM.
   */
  override disconnectedCallback() {
    super.disconnectedCallback();

    this.browserProxy_.removeNavigationStateListener(
        this.navigationStateListenerHandle_);

    this.metricsRecorder_.stopObserving();
    const reload = this.shadowRoot.querySelector<HTMLElement>('#reload');
    if (reload) {
      this.trackedElementManager_.stopTracking(reload);
    }
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
      this.browserProxy_.toolbarUIHandler.onPageInitialized();
    });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'toolbar-app': ToolbarAppElement;
  }
}

customElements.define(ToolbarAppElement.is, ToolbarAppElement);
