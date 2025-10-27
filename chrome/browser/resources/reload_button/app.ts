// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/icons.html.js';
import '/strings.m.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import {BrowserProxyImpl} from './browser_proxy.js';
import type {BrowserProxy} from './browser_proxy.js';
import {MetricsRecorder} from './metrics_recorder.js';

const RELOAD_BUTTON_TOOLTIP_RELOAD_WITH_MENU =
    'reloadButtonTooltipReloadWithMenu';
const RELOAD_BUTTON_TOOLTIP_RELOAD = 'reloadButtonTooltipReload';
const RELOAD_BUTTON_TOOLTIP_STOP = 'reloadButtonTooltipStop';

const LONG_PRESS_TIMER_THRESHOLD_MS = 500;

export class ReloadButtonAppElement extends CrLitElement {
  private browserProxy_: BrowserProxy;
  private metricsRecorder_: MetricsRecorder;

  constructor() {
    super();
    this.browserProxy_ = BrowserProxyImpl.getInstance();
    this.metricsRecorder_ = new MetricsRecorder(this.browserProxy_);
    const callbackRouter = this.browserProxy_.callbackRouter;
    callbackRouter.setReloadButtonState.addListener(
        (isLoading: boolean, isMenuEnabled: boolean) => {
          this.metricsRecorder_.onChangeVisibleMode(
              MetricsRecorder.getVisibleMode(this.isLoading_),
              MetricsRecorder.getVisibleMode(isLoading));
          this.isLoading_ = isLoading;
          this.isMenuEnabled_ = isMenuEnabled;
          this.tooltip_ = loadTimeData.getString(
              isLoading ?
                  RELOAD_BUTTON_TOOLTIP_STOP :
                  (isMenuEnabled ? RELOAD_BUTTON_TOOLTIP_RELOAD_WITH_MENU :
                                   RELOAD_BUTTON_TOOLTIP_RELOAD));
        });
    ColorChangeUpdater.forDocument().start();
  }

  static get is() {
    return 'reload-button-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      isLoading_: {state: true, type: Boolean},
      tooltip_: {state: true, type: String},
    };
  }

  protected accessor isLoading_: boolean = false;
  protected accessor tooltip_: string =
      loadTimeData.getString(RELOAD_BUTTON_TOOLTIP_RELOAD);
  private isLongPressed_: boolean = false;
  private longPressTimer_: number = 0;
  private isMenuEnabled_: boolean = false;

  /**
   * Sets up event listeners and the PerformanceObserver when the element is
   * added to the DOM.
   */
  override connectedCallback() {
    super.connectedCallback();

    this.metricsRecorder_.startObserving();
  }

  /**
   * Cleans up event listeners and the PerformanceObserver when the element is
   * removed from the DOM.
   */
  override disconnectedCallback() {
    super.disconnectedCallback();

    this.metricsRecorder_.stopObserving();
  }

  protected onReloadButtonPointerDown_(e: MouseEvent) {
    if (e.button !== 0) {
      // The TypeScript code should only handle the left-click.
      return;
    }

    // Reset the long press tracker.
    this.isLongPressed_ = false;
    clearTimeout(this.longPressTimer_);

    if (this.isLoading_) {
      // No long press handler for the "stop loading" case.
      return;
    }

    this.longPressTimer_ = setTimeout(() => {
      // When the long press is triggered and handled, mark `isLongPressed_`
      // as true, so that it won't be treated as a normal click.
      this.isLongPressed_ = true;
      if (this.isMenuEnabled_) {
        BrowserProxyImpl.getInstance().handler.showContextMenu(
            e.offsetX, e.offsetY);
      }
    }, LONG_PRESS_TIMER_THRESHOLD_MS);
  }

  protected onReloadButtonPointerUp_(e: MouseEvent) {
    this.metricsRecorder_.onButtonPressedStart(e);
    if (this.isLongPressed_) {
      // If the long press is already handled, skip the rest.
      this.isLongPressed_ = false;
      return;
    }
    this.metricsRecorder_.onChangeVisibleMode(
        MetricsRecorder.getVisibleMode(this.isLoading_),
        MetricsRecorder.getVisibleMode(!this.isLoading_));

    clearTimeout(this.longPressTimer_);

    if (this.isLoading_) {
      BrowserProxyImpl.getInstance().handler.stopReload();
    } else {
      // If the shift or ctrl key is pressed, we should reload with cache
      // ignored.
      BrowserProxyImpl.getInstance().handler.reload(e.shiftKey || e.ctrlKey);
    }
    // Update the renderer in advance to avoid the delay.
    this.isLoading_ = !this.isLoading_;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'reload-button-app': ReloadButtonAppElement;
  }
}

customElements.define(ReloadButtonAppElement.is, ReloadButtonAppElement);
