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
import {TimerHelper} from './timer_helper.js';
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
      accName_: {type: String},
      state: {type: Object},
      tooltip: {type: String, reflect: true},
      showStopIcon: {type: Boolean, reflect: true},
      isDisabled: {type: Boolean, reflect: true},
    };
  }

  protected accessor state: ReloadControlState = {
    // While this will be overwritten anyways, this matches the default value on
    // some platforms.
    doubleClickInterval: {microseconds: BigInt(500 * 1000)},

    canShowMenu: false,
    isNavigationLoading: false,
    isContextMenuVisible: false,
    stateToken: 0,
  };
  protected accessor tooltip: string =
      loadTimeData.getString(RELOAD_BUTTON_TOOLTIP_RELOAD);
  protected accessor accName_: string =
      loadTimeData.getString(RELOAD_BUTTON_ACC_NAME_RELOAD);
  protected pressHandler_: PressHandler;

  // True when the stop icon should be shown instead of the reload icon. In
  // general, `showStopIcon` should match `state.isNavigationLoading`, except
  // while one of the "debounce" timers is running.
  protected accessor showStopIcon: boolean = false;

  // Whether the reload button should be disabled. True only while the
  // `disableStopIconTimer_` is running.
  protected accessor isDisabled: boolean = false;

  // Timer started when the reload button is pressed while showing the reload
  // icon. While running, the reload icon will continue to be displayed instead
  // of the stop icon, and left clicks on the icon will be ignored. Once the
  // timer expires or the load completes, the timer will stop and the updated
  // icon will be displayed, and clicks will be respected again.
  protected doubleClickReloadIconTimer_: TimerHelper = new TimerHelper();

  // This is exposed so tests can modify it.
  protected modeSwitchIntervalMs_: number = 1350;

  // Timer started when the mouse is over the stop icon, and loading stops for
  // any reason other than the user clicking the stop icon. During this time,
  // the stop icon continues to be displayed, but is disabled. Once the timer
  // expires, the mouse moves off the icon, or loading starts again for any
  // reason, the timer will be stopped and the button will be enabled, leaving
  // this state.
  private disableStopIconTimer_: TimerHelper = new TimerHelper();

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
          ContextMenuType.kReload, getContextMenuPosition(this), source);
    }
  }

  private onShortPress_(e: MouseEvent) {
    const isLeftClick = e.button === BUTTON_LEFT;
    // Handle the visible state changes only for left-click.
    if (isLeftClick && !e.metaKey) {
      // Do nothing if timer is still running.
      if (this.doubleClickReloadIconTimer_.isRunning()) {
        return;
      }

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

    if (isLeftClick && !e.metaKey) {
      // Update the renderer in advance to avoid the delay.
      this.state.isNavigationLoading = !this.state.isNavigationLoading;

      if (this.showStopIcon) {
        // If the user clicked the stop button, immediately update to the reload
        // button.
        this.updateState_(/*force=*/ true);
      } else {
        // If the reload button was showing, start the click timer, which will
        // cause future presses to be ignored until it expires.
        this.doubleClickReloadIconTimer_.setTimeout(() => {
          this.updateState_(/*force=*/ true);
        }, Number(this.state.doubleClickInterval.microseconds) / 1000);
      }
    }
  }

  protected onClick_(e: MouseEvent) {
    // Only keyboard `click` (Enter/Space) are handled here, which triggers a
    // left-click equivalent. Keyboard 'click' has detail === 0.
    if (e.detail === 0) {
      this.onShortPress_(e);
    }
  }

  private updateState_(force: boolean) {
    // If `force` was not passed in, and the pointer is hovering over the
    // reload button, need to decide if can update the button immediately or
    // not.
    if (!force &&
        this.renderRoot.querySelector('cr-icon-button')?.matches(':hover')) {
      if (this.state.isNavigationLoading) {
        // If the navigation is loading, and thus we want to be displaying the
        // stop button, and we're still in the double-click period for clicking
        // the reload button (which means the reload button is still displayed),
        // ignore the message entirely. We'll start showing the stop button once
        // the timer expires.
        if (this.doubleClickReloadIconTimer_.isRunning()) {
          return;
        }

        // If the click timer isn't running, then we'll immediately update.
      } else {
        // If not loading and the timer to show the reload button is still
        // running, continue waiting on the timer.
        if (this.disableStopIconTimer_.isRunning()) {
          return;
        }

        // If we're showing the stop button, and should now show the reload
        // button, disable the button and reenable it on a timer, to prevent
        // accidentally triggering a reload.
        if (this.showStopIcon) {
          this.isDisabled = true;
          this.disableStopIconTimer_.setTimeout(() => {
            // This will conveniently delete `disableStopIconTimer_`.
            this.updateState_(/*force=*/ true);
          }, this.modeSwitchIntervalMs_);
          return;
        }
      }
    }

    // Cancel any running timers, enable the button, and update the displayed
    // icon.
    this.doubleClickReloadIconTimer_.clearTimeout();
    this.disableStopIconTimer_.clearTimeout();
    this.isDisabled = false;
    this.showStopIcon = this.state.isNavigationLoading;
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

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedPrivateProperties.has('state')) {
      const previousState =
          changedPrivateProperties.get('state') as ReloadControlState |
          undefined;
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
      this.updateState_(/*force=*/ !previousState ||
                        this.state.stateToken !== previousState.stateToken);
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
   * Handles pointer release. Records metrics and delegates to PressHandler
   * to evaluate whether the interaction was a short or long press.
   * If it's from the right mouse click, it's not handled from the Javascript.
   * @param e the PointerEvent associated with the click.
   * @returns
   */
  protected onPointerup_(e: PointerEvent) {
    if (e.button !== BUTTON_RIGHT) {
      this.metricsRecorder_.onButtonPressedStart(e);
    }
    this.pressHandler_.onPointerup(e);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'reload-button': ReloadButtonElement;
  }
}

customElements.define(ReloadButtonElement.is, ReloadButtonElement);
