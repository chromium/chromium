// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/icons.html.js';
import '/strings.m.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import type {MenuSourceType} from '//resources/mojo/ui/base/mojom/menu_source_type.mojom-webui.js';
import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {BrowserProxyImpl, ContextMenuType} from './browser_proxy.js';
import type {BrowserProxy, ReloadControlState} from './browser_proxy.js';
import {MetricsRecorder} from './metrics_recorder.js';
import {getCss} from './reload_button.css.js';
import {getHtml} from './reload_button.html.js';
import {BUTTON_LEFT, BUTTON_RIGHT, getClickDispositionFlags, getContextMenuPosition, PressHandler} from './toolbar_button.js';

// go/keep-sorted start
const RELOAD_BUTTON_ACC_NAME_RELOAD = 'reloadButtonAccNameReload';
const RELOAD_BUTTON_TOOLTIP_RELOAD = 'reloadButtonTooltipReload';
const RELOAD_BUTTON_TOOLTIP_RELOAD_WITH_MENU =
    'reloadButtonTooltipReloadWithMenu';
const RELOAD_BUTTON_TOOLTIP_STOP = 'reloadButtonTooltipStop';
// go/keep-sorted end

export class ReloadButtonElement extends CrLitElement {
  static get is() {
    return 'reload-button';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      state: {type: Object},
      tooltip: {type: String, reflect: true},
    };
  }

  protected accessor state: ReloadControlState = {
    canShowMenu: false,
    isNavigationLoading: false,
    isContextMenuVisible: false,
  };
  protected accessor tooltip: string =
      loadTimeData.getString(RELOAD_BUTTON_TOOLTIP_RELOAD);
  protected accName_: string =
      loadTimeData.getString(RELOAD_BUTTON_ACC_NAME_RELOAD);
  protected pressHandler_: PressHandler;

  private browserProxy_: BrowserProxy;
  private metricsRecorder_: MetricsRecorder;

  constructor() {
    super();
    this.browserProxy_ = BrowserProxyImpl.getInstance();
    this.metricsRecorder_ = new MetricsRecorder(this.browserProxy_);
    this.pressHandler_ = new PressHandler(
        this.onLongPress_.bind(this), this.onShortPress_.bind(this),
        /*enableMacContextClick=*/ false);
    ColorChangeUpdater.forDocument().start();
  }

  private onLongPress_(source: MenuSourceType) {
    if (this.state.canShowMenu) {
      this.browserProxy_.toolbarUIHandler.showContextMenu(
          ContextMenuType.kReload, this.contextMenuPosition(), source);
    }
  }

  private onShortPress_(e: PointerEvent) {
    // Handle the visible state changes only for left-click.
    if (e.button === BUTTON_LEFT && !e.metaKey) {
      this.metricsRecorder_.onChangeVisibleMode(
          MetricsRecorder.getVisibleMode(this.state.isNavigationLoading),
          MetricsRecorder.getVisibleMode(!this.state.isNavigationLoading));
    }

    if (this.state.isNavigationLoading) {
      this.browserProxy_.browserControlsHandler.stopLoad();
    } else {
      // If the shift or ctrl key is pressed, we should reload with cache
      // bypassed.
      this.browserProxy_.browserControlsHandler.reloadFromClick(
          /*bypass_cache=*/ e.shiftKey || e.ctrlKey,
          getClickDispositionFlags(
              e, {ignoreCtrlKey: true, ignoreShiftKey: true}));
    }

    if (e.button === BUTTON_LEFT && !e.metaKey) {
      // Update the renderer in advance to avoid the delay.
      this.state.isNavigationLoading = !this.state.isNavigationLoading;
    }
  }

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

  override willUpdate(changedProperties: PropertyValues<this>): void {
    super.willUpdate(changedProperties);

    const props = changedProperties as Map<string, any>;

    if (props.has('state')) {
      const previousState = props.get('state') as typeof this.state | undefined;
      if (previousState) {
        this.metricsRecorder_.onChangeVisibleMode(
            MetricsRecorder.getVisibleMode(previousState.isNavigationLoading),
            MetricsRecorder.getVisibleMode(this.state.isNavigationLoading));
      }
      this.tooltip = loadTimeData.getString(
          this.state.isNavigationLoading ?
              RELOAD_BUTTON_TOOLTIP_STOP :
              (this.state.canShowMenu ? RELOAD_BUTTON_TOOLTIP_RELOAD_WITH_MENU :
                                        RELOAD_BUTTON_TOOLTIP_RELOAD));
    }
  }

  /**
   * See `onPointerup_` for the click event handling logic.
   * @param e the PointerEvent associated with the click.
   * @returns
   */
  protected onPointerdown_(e: PointerEvent) {
    this.pressHandler_.onPointerdown(e, this.state.isNavigationLoading);
  }

  /**
   * Handles the mouse click event.
   * - If it's from the right mouse click, it's not handled from the Javascript.
   * - If it's a single click:
   *    - if the page is already in loading process, it should stop the process.
   *    - if the page is not loading:
   *        - if it's from the left mouse click, it should trigger the page
   *          reload, so the loading state should be updated accordingly.
   *        - if it's from the middle mouse click, it should open the same page
   *          from another background tab, and the loading state of the current
   *          tab remains unchanged.
   * - If it's a long press with a duration longer than
   *   `LONG_PRESS_TIMER_THRESHOLD_MS`, no matter it's a left click or middle
   *   click, it should triggers the context menu display if the devtools is
   *   open (see `onPointerdown_`).
   * @param e the PointerEvent associated with the click.
   * @returns
   */
  protected onPointerup_(e: PointerEvent) {
    if (e.button !== BUTTON_RIGHT) {
      this.metricsRecorder_.onButtonPressedStart(e);
    }
    this.pressHandler_.onPointerup(e);
  }

  protected contextMenuPosition() {
    return getContextMenuPosition(this);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'reload-button': ReloadButtonElement;
  }
}

customElements.define(ReloadButtonElement.is, ReloadButtonElement);
