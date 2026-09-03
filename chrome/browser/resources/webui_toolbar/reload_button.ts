// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/icons.html.js';
import '/strings.m.js';

import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import type {MenuSourceType} from '//resources/mojo/ui/base/mojom/menu_source_type.mojom-webui.js';
import {ReloadInputType} from '/shared/browser_controls_api.mojom-webui.js';
import type {ReloadInteractionMetadata} from '/shared/browser_controls_api.mojom-webui.js';
import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {BrowserProxyImpl, ContextMenuType} from './browser_proxy.js';
import type {BrowserProxy, ReloadControlState} from './browser_proxy.js';
import {ReloadButtonInputType} from './metrics_recorder.js';
import {getCss} from './reload_button.css.js';
import {getHtml} from './reload_button.html.js';
import {TimerHelper} from './timer_helper.js';
import {BUTTON_LEFT, getContextMenuPosition, getEventDispositionFlags, HelpBubbleAnchorMixin, playIconAnimation, PressHandler, roundedIconsEnabled} from './toolbar_button.js';

// go/keep-sorted start
const RELOAD_BUTTON_ACC_NAME_RELOAD = 'reloadButtonAccNameReload';
const RELOAD_BUTTON_TOOLTIP_RELOAD = 'reloadButtonTooltipReload';
const RELOAD_BUTTON_TOOLTIP_RELOAD_WITH_MENU =
    'reloadButtonTooltipReloadWithMenu';
const RELOAD_BUTTON_TOOLTIP_STOP = 'reloadButtonTooltipStop';
// go/keep-sorted end
const INPUT_COUNT_HISTOGRAM = 'InitialWebUI.ReloadButton.InputCount';

const ReloadButtonElementBase = HelpBubbleAnchorMixin(CrLitElement);

export interface ReloadButtonElement {
  $: {
    button: CrIconButtonElement,
  };
}

export class ReloadButtonElement extends ReloadButtonElementBase {
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
      ...super.properties,
      accName_: {type: String},
      state: {type: Object},
      tooltip: {type: String, reflect: true},
      showStopIcon: {type: Boolean, reflect: true},
      isDisabled: {type: Boolean, reflect: true},
      touchUi: {type: Boolean},
      glowUpEnabled: {type: Boolean},
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

  // Whether the reload button should be disabled. True while the
  // `disableStopIconTimer_` is running, or when the button is temporarily
  // disabled during Glow Up animations to prevent interruption.
  protected accessor isDisabled: boolean = false;

  accessor touchUi: boolean = false;
  accessor glowUpEnabled: boolean =
      loadTimeData.getBoolean('enableReloadGlowUp');

  // True if state transitions (e.g. Reload <-> Stop) should be animated.
  // This is set to true on user click and reset to false after the animation
  // is triggered, ensuring we only animate user-initiated actions.
  private animateTransitions_ = false;

  // Represents the currently playing animation state.
  // - 'start': Transitioning from Reload to Stop.
  // - 'end': Transitioning from Stop to Reload.
  // - 'none': No animation is active.
  private activeAnimation_: 'start'|'end'|'none' = 'none';

  // Represents the next animation state to play after the current one
  // completes.
  // - 'end': Queue transition back to Reload after 'start' finishes.
  // - 'none': No pending animation.
  private pendingAnimation_: 'end'|'none' = 'none';

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

  constructor() {
    super();
    this.browserProxy_ = BrowserProxyImpl.getInstance();
    this.pressHandler_ = new PressHandler(
        this.onLongPress_.bind(this), this.onShortPress_.bind(this));
    ColorChangeUpdater.forDocument().start();
  }

  // We listen to 'pointerleave' on the host element instead of the internal
  // button. When the button is disabled, it gets 'pointer-events: none',
  // which triggers an artificial 'pointerleave' event even if the mouse is
  // still hovering. Listening on the host (which is never disabled) and
  // checking host hover state in updateState_ allows us to ignore these
  // fake pointerleave events (which can propagate to the host on some platforms
  // like Mac).
  private boundOnPointerleave_ = this.onPointerleave_.bind(this);

  override connectedCallback() {
    super.connectedCallback();
    this.addEventListener('pointerleave', this.boundOnPointerleave_);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.removeEventListener('pointerleave', this.boundOnPointerleave_);
  }

  private onLongPress_(source: MenuSourceType) {
    if (this.state.canShowMenu) {
      this.browserProxy_.toolbarUIHandler.showContextMenu(
          ContextMenuType.kReload, getContextMenuPosition(this), source,
          /*showMenuToken=*/ null);
    }
  }

  private onShortPress_(e: MouseEvent) {
    // Ignore clicks if the button is disabled (e.g. during hover protection).
    // This is also necessary to block programmatically dispatched events in
    // tests that bypass the HTML disabled state.
    if (this.isDisabled) {
      return;
    }
    const isLeftClick = e.button === BUTTON_LEFT;
    // Handle the visible state changes only for left-click.
    if (isLeftClick && !e.metaKey) {
      // Do nothing if timer is still running.
      if (this.doubleClickReloadIconTimer_.isRunning()) {
        return;
      }
    }

    const isKeyboard = e.type === 'click';
    const recordedInputType = isKeyboard ? ReloadButtonInputType.KEY_PRESS :
                                           ReloadButtonInputType.MOUSE_RELEASE;
    this.browserProxy_.recordInHistogram(
        INPUT_COUNT_HISTOGRAM, recordedInputType, ReloadButtonInputType.COUNT);

    if (this.state.isNavigationLoading) {
      this.browserProxy_.browserControlsHandler.stopLoad();
    } else {
      // If the shift or ctrl key is pressed, we should reload with cache
      // bypassed.
      const metadata = this.getReloadMetadata_(e);
      this.browserProxy_.browserControlsHandler.reloadFromClick(
          /*bypass_cache=*/ e.shiftKey || e.ctrlKey,
          getEventDispositionFlags(
              e, {ignoreCtrlKey: true, ignoreShiftKey: true}),
          metadata);
    }

    if (isLeftClick && !e.metaKey) {
      this.animateTransitions_ = true;
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

  /**
   * Constructs the interaction metadata from the mouse/pointer event.
   * Reconstructs the relative timestamp offset and determines the input
   * modality.
   */
  private getReloadMetadata_(e: MouseEvent): ReloadInteractionMetadata|null {
    const sourceCapabilities =
        (e as unknown as {
          sourceCapabilities?: {firesTouchEvents?: boolean},
        }).sourceCapabilities;
    const isTouch = (e instanceof PointerEvent && e.pointerType === 'touch') ||
        (!!sourceCapabilities && sourceCapabilities.firesTouchEvents);
    if (isTouch) {
      return null;
    }
    const interactionTimeOffset = BigInt(Math.round(e.timeStamp * 1000));
    const isKeyboard = e.type === 'click';
    const inputType =
        isKeyboard ? ReloadInputType.kKeyPress : ReloadInputType.kMouseRelease;
    return {
      interactionTimeOffset: {microseconds: interactionTimeOffset},
      inputType: inputType,
    };
  }

  protected onClick_(e: MouseEvent) {
    // Only keyboard `click` (Enter/Space) are handled here, which triggers a
    // left-click equivalent. Keyboard 'click' has detail === 0.
    if (e.detail === 0) {
      this.onShortPress_(e);
    }
  }

  private updateState_(force: boolean) {
    // If the navigation finishes while we are still animating the transition
    // to the Stop state (activeAnimation_ is 'start'), temporarily disable the
    // button to prevent clicks, and wait for the animation to complete before
    // transitioning back to Reload. This matches the Views behavior of not
    // interrupting the animation abruptly and keeping the button disabled,
    // adapted to WebUI's simplified animation chaining model.
    if (this.glowUpEnabled && !force && this.activeAnimation_ === 'start') {
      if (!this.state.isNavigationLoading) {
        this.isDisabled = true;
      }
      return;
    }

    // If `force` was not passed in, and the pointer is hovering over the
    // reload button, need to decide if can update the button immediately or
    // not.
    if (!force && this.matches(':hover')) {
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
    const oldShowStopIcon = this.showStopIcon;
    this.showStopIcon = this.state.isNavigationLoading;

    // Determine if we should play animation
    // Normal transition if state changed.
    let playAnimation = this.glowUpEnabled && this.animateTransitions_ &&
        oldShowStopIcon !== this.showStopIcon;

    // Double animation if we clicked reload but stayed in reload (fast load).
    const playDoubleAnimation = this.glowUpEnabled &&
        this.animateTransitions_ && !this.showStopIcon && !oldShowStopIcon &&
        this.activeAnimation_ === 'none';

    if (playDoubleAnimation) {
      playAnimation = true;
    }

    if (playAnimation) {
      if (playDoubleAnimation) {
        this.activeAnimation_ = 'start';
        this.pendingAnimation_ = 'end';
      } else {
        this.activeAnimation_ = this.showStopIcon ? 'start' : 'end';
      }
    } else {
      // Only reset to 'none' if we are not currently in the middle of that
      // animation. This prevents force-stopping a running animation if
      // updateState_ is called with the same target state (e.g. on pointer
      // leave).
      if (!((this.activeAnimation_ === 'start' &&
             (this.showStopIcon || this.pendingAnimation_ === 'end')) ||
            (this.activeAnimation_ === 'end' && !this.showStopIcon))) {
        this.activeAnimation_ = 'none';
        this.pendingAnimation_ = 'none';
      }
    }

    if (!this.showStopIcon && !playAnimation) {
      // Reset flag when transitioning to reload (matching C++ case
      // Mode::kReload)
      this.animateTransitions_ = false;
    }

    if (playAnimation && this.hasUpdated) {
      this.playAnimation_();
    }
  }


  override willUpdate(changedProperties: PropertyValues<this>): void {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedPrivateProperties.has('state')) {
      const previousState =
          changedPrivateProperties.get('state') as ReloadControlState |
          undefined;
      this.updateTooltip_();
      this.updateState_(/*force=*/ !previousState ||
                        this.state.stateToken !== previousState.stateToken);
    }

    if (changedPrivateProperties.has('hasHelpBubble')) {
      this.updateTooltip_();
    }
  }

  private updateTooltip_() {
    this.tooltip = this.adjustTooltipForHelpBubble(loadTimeData.getString(
        this.state.isNavigationLoading ?
            RELOAD_BUTTON_TOOLTIP_STOP :
            (this.state.canShowMenu ? RELOAD_BUTTON_TOOLTIP_RELOAD_WITH_MENU :
                                      RELOAD_BUTTON_TOOLTIP_RELOAD)));
  }

  protected getIronIcon_(): string {
    if (this.glowUpEnabled) {
      if (this.activeAnimation_ === 'start') {
        return 'webui-toolbar:reload_start_glow_up';
      }
      if (this.activeAnimation_ === 'end') {
        return 'webui-toolbar:reload_end_glow_up';
      }
    }

    if (this.showStopIcon) {
      if (roundedIconsEnabled()) {
        return 'webui-toolbar:close';
      } else {
        return this.touchUi ? 'webui-toolbar:navigate_stop_touch_old' :
                              'webui-toolbar:navigate_stop_chrome_refresh_old';
      }
    } else {
      if (roundedIconsEnabled()) {
        return 'webui-toolbar:refresh';
      } else {
        return this.touchUi ? 'webui-toolbar:reload_touch_old' :
                              'webui-toolbar:reload_chrome_refresh_old';
      }
    }
  }

  private playAnimation_() {
    const isStart = this.activeAnimation_ === 'start';
    this.updateComplete.then(() => {
      playIconAnimation(this.$.button);
      this.listenToAnimationEnd_(isStart);
    });
  }

  /**
   * Listens to the SMIL animation end event on the SVG element.
   * Resets animation states once the animation finishes.
   * @param isStart True if we are listening to the 'start' (Reload->Stop)
   *     animation.
   */
  private listenToAnimationEnd_(isStart: boolean) {
    const animate =
        this.$.button.shadowRoot.querySelector('cr-icon')
            ?.shadowRoot?.querySelector('animate[begin="indefinite"]');
    if (!animate) {
      this.activeAnimation_ = 'none';
      this.pendingAnimation_ = 'none';
      return;
    }
    const handler = () => {
      animate.removeEventListener('endEvent', handler);
      if (isStart) {
        this.onStartAnimationEnd_();
      } else {
        this.onEndAnimationEnd_();
      }
    };
    animate.addEventListener('endEvent', handler);
  }

  /**
   * Called when the 'start' animation (Reload -> Stop transition) completes.
   * Handles starting the pending 'end' animation if the load has already
   * finished, or resetting the animation state.
   */
  private onStartAnimationEnd_() {
    if (this.activeAnimation_ !== 'start') {
      return;
    }
    if (this.pendingAnimation_ === 'end') {
      this.pendingAnimation_ = 'none';
      if (!this.state.isNavigationLoading) {
        this.activeAnimation_ = 'end';
        this.requestUpdate();
        this.playAnimation_();
        return;
      }
    }

    this.activeAnimation_ = 'none';
    this.requestUpdate();

    if (!this.state.isNavigationLoading) {
      this.updateState_(/*force=*/ false);
    }
  }

  private onEndAnimationEnd_() {
    if (this.activeAnimation_ !== 'end') {
      return;
    }
    this.activeAnimation_ = 'none';
    this.pendingAnimation_ = 'none';
    this.animateTransitions_ = false;
    this.requestUpdate();
    this.updateState_(/*force=*/ false);
  }

  /**
   * See `onPointerup_` for the click event handling logic.
   * @param e the PointerEvent associated with the click.
   * @returns
   */
  protected onPointerdown_(e: PointerEvent) {
    // Ignore pointer events if the button is disabled (e.g. during hover
    // protection). Natively, disabled elements still receive pointer events
    // (unlike click events), so we must manually block them here to prevent
    // capturing pointer events or triggering long-press menu while disabled.
    if (this.isDisabled) {
      return;
    }
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
    this.pressHandler_.onPointerup(e);
  }

  /**
   * Handles pointer leave. If the context menu is not showing, it triggers an
   * immediate update to transition back to the reload state if loading has
   * finished, bypassing the hover protection delay. This matches the C++ Views
   * `OnMouseExited` behavior.
   */
  private onPointerleave_() {
    if (!this.state.isContextMenuVisible) {
      this.updateState_(/*force=*/ false);
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'reload-button': ReloadButtonElement;
  }
}

customElements.define(ReloadButtonElement.is, ReloadButtonElement);
