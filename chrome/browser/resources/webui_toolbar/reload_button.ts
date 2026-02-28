// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/icons.html.js';
import '/strings.m.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {MenuSourceType} from '//resources/mojo/ui/base/mojom/menu_source_type.mojom-webui.js';
import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {BrowserProxyImpl, ClickDispositionFlag, ContextMenuType} from './browser_proxy.js';
import type {BrowserProxy, ReloadControlState} from './browser_proxy.js';
import {MetricsRecorder} from './metrics_recorder.js';
import {getCss} from './reload_button.css.js';
import {getHtml} from './reload_button.html.js';
import {getContextMenuPosition} from './toolbar_button.js';

// go/keep-sorted start
const RELOAD_BUTTON_ACC_NAME_RELOAD = 'reloadButtonAccNameReload';
const RELOAD_BUTTON_TOOLTIP_RELOAD = 'reloadButtonTooltipReload';
const RELOAD_BUTTON_TOOLTIP_RELOAD_WITH_MENU =
    'reloadButtonTooltipReloadWithMenu';
const RELOAD_BUTTON_TOOLTIP_STOP = 'reloadButtonTooltipStop';
// go/keep-sorted end

const BUTTON_LEFT = 0;
const BUTTON_MIDDLE = 1;
const BUTTON_RIGHT = 2;

const LONG_PRESS_TIMER_THRESHOLD_MS = 500;

export class ReloadButtonAppElement extends CrLitElement {
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
      state: {type: Object},
      tooltip: {type: String, reflect: true},
    };
  }

  protected accessor state: ReloadControlState = {
    isDevtoolsConnected: false,
    isNavigationLoading: false,
    isContextMenuVisible: false,
  };
  protected accessor tooltip: string =
      loadTimeData.getString(RELOAD_BUTTON_TOOLTIP_RELOAD);
  protected accName_: string =
      loadTimeData.getString(RELOAD_BUTTON_ACC_NAME_RELOAD);
  private isLongPressed_: boolean = false;
  private longPressTimer_: number = 0;

  private browserProxy_: BrowserProxy;
  private metricsRecorder_: MetricsRecorder;

  constructor() {
    super();
    this.browserProxy_ = BrowserProxyImpl.getInstance();
    this.metricsRecorder_ = new MetricsRecorder(this.browserProxy_);
    ColorChangeUpdater.forDocument().start();
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
              (this.state.isContextMenuVisible ?
                   RELOAD_BUTTON_TOOLTIP_RELOAD_WITH_MENU :
                   RELOAD_BUTTON_TOOLTIP_RELOAD));
    }
  }

  /**
   * See `onReloadButtonPointerup_` for the click event handling logic.
   * @param e the MouseEvent associated with the click.
   * @returns
   */
  protected onReloadButtonPointerdown_(e: MouseEvent) {
    if (e.button === BUTTON_RIGHT) {
      // The TypeScript code should only handle long press for the
      // left-click/middle-click.
      return;
    }

    // Reset the long press tracker.
    this.isLongPressed_ = false;
    clearTimeout(this.longPressTimer_);

    if (this.state.isNavigationLoading) {
      // No long press handler for the "stop loading" case.
      return;
    }

    this.longPressTimer_ = setTimeout(() => {
      // When the long press is triggered and handled, mark `isLongPressed_`
      // as true, so that it won't be treated as a normal click.
      this.isLongPressed_ = true;
      if (this.state.isDevtoolsConnected) {
        BrowserProxyImpl.getInstance().toolbarUIHandler.showContextMenu(
            ContextMenuType.kReload, this.contextMenuPosition(),
            MenuSourceType.kLongPress);
      }
    }, LONG_PRESS_TIMER_THRESHOLD_MS);
  }

  /**
   * Generate the list of `ClickDispositionFlag`s based on the `MouseEvent`.
   */
  private generateFlags(e: MouseEvent): ClickDispositionFlag[] {
    const flags: ClickDispositionFlag[] = [];
    if (e.button === BUTTON_MIDDLE) {
      flags.push(ClickDispositionFlag.kMiddleMouseButton);
    }
    if (e.altKey) {
      flags.push(ClickDispositionFlag.kAltKeyDown);
    }
    if (e.metaKey) {
      flags.push(ClickDispositionFlag.kMetaKeyDown);
    }
    return flags;
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
   *   open (see `onReloadButtonPointerdown_`).
   * @param e the MouseEvent associated with the click.
   * @returns
   */
  protected onReloadButtonPointerup_(e: MouseEvent) {
    if (e.button === BUTTON_RIGHT) {
      return;
    }

    this.metricsRecorder_.onButtonPressedStart(e);
    if (this.isLongPressed_) {
      // If the long press is already handled, skip the rest.
      this.isLongPressed_ = false;
      return;
    }

    // Handle the visible state changes only for left-click.
    if (e.button === BUTTON_LEFT && !e.metaKey) {
      this.metricsRecorder_.onChangeVisibleMode(
          MetricsRecorder.getVisibleMode(this.state.isNavigationLoading),
          MetricsRecorder.getVisibleMode(!this.state.isNavigationLoading));
    }

    clearTimeout(this.longPressTimer_);

    if (this.state.isNavigationLoading) {
      BrowserProxyImpl.getInstance().browserControlsHandler.stopLoad();
    } else {
      // If the shift or ctrl key is pressed, we should reload with cache
      // bypassed.
      BrowserProxyImpl.getInstance().browserControlsHandler.reloadFromClick(
          /*bypass_cache=*/ e.shiftKey || e.ctrlKey, this.generateFlags(e));
    }

    if (e.button === BUTTON_LEFT && !e.metaKey) {
      // Update the renderer in advance to avoid the delay.
      this.state.isNavigationLoading = !this.state.isNavigationLoading;
    }
  }

  protected onContextmenu_(e: PointerEvent) {
    if (this.state.isDevtoolsConnected) {
      BrowserProxyImpl.getInstance().toolbarUIHandler.showContextMenu(
          ContextMenuType.kReload, this.contextMenuPosition(),
          MenuSourceType.kMouse);
    }
    e.preventDefault();
  }

  protected contextMenuPosition() {
    return getContextMenuPosition(this);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'reload-button-app': ReloadButtonAppElement;
  }
}

customElements.define(ReloadButtonAppElement.is, ReloadButtonAppElement);
