// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/icons.html.js';

import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import type {MenuSourceType} from '//resources/mojo/ui/base/mojom/menu_source_type.mojom-webui.js';

import {getCss} from './back_forward_button.css.js';
import {getHtml} from './back_forward_button.html.js';
import {BrowserProxyImpl, ContextMenuType} from './browser_proxy.js';
import type {BackForwardButtonState, BrowserProxy} from './browser_proxy.js';
import {OverflowableButtonMixin} from './overflowable_button.js';
import {getContextMenuPosition, getEventDispositionFlags, HelpBubbleAnchorMixin, playIconAnimation, PressHandler, roundedIconsEnabled} from './toolbar_button.js';

const BackForwardButtonElementBase =
    HelpBubbleAnchorMixin(OverflowableButtonMixin(CrLitElement));

export interface BackForwardButtonElement {
  $: {
    button: CrIconButtonElement,
    buttonWrapper: HTMLElement,
  };
}

export class BackForwardButtonElement extends BackForwardButtonElementBase {
  static get is() {
    return 'back-forward-button';
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
      direction: {type: String},
      state: {type: Object},
      windowIsMaximizedOrFullscreen: {type: Boolean},
      touchUi: {type: Boolean},
      glowUpEnabled: {type: Boolean},
    };
  }

  accessor direction: 'back'|'forward' = 'back';
  override accessor state: BackForwardButtonState = {
    enabled: false,
    shouldBeShown: true,
    isContextMenuVisible: false,
  };
  accessor windowIsMaximizedOrFullscreen: boolean = false;
  accessor touchUi: boolean = false;
  accessor glowUpEnabled: boolean =
      loadTimeData.getBoolean('enableBackForwardGlowUp');

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);
    if (changedProperties.has('windowIsMaximizedOrFullscreen')) {
      this.toggleAttribute(
          'window-is-maximized-or-fullscreen',
          this.windowIsMaximizedOrFullscreen);
    }
  }

  private manualRippleTriggered_: boolean = false;
  // True when the button is actively playing the Glow Up click animation.
  // While true, the icon is switched to the animated version. Once the
  // animation completes, this is reset to false to fall back to the static
  // icon.
  private animating_: boolean = false;
  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();
  protected pressHandler_: PressHandler = new PressHandler(
      this.onLongPress_.bind(this), this.onShortPress_.bind(this));

  private get contextMenuType_(): ContextMenuType {
    return this.direction === 'back' ? ContextMenuType.kBack :
                                       ContextMenuType.kForward;
  }

  private onLongPress_(source: MenuSourceType) {
    if (!this.state.enabled) {
      return;
    }
    this.browserProxy_.toolbarUIHandler.showContextMenu(
        this.contextMenuType_, getContextMenuPosition(this), source,
        /*showMenuToken=*/ null);
  }

  private onShortPress_(e: MouseEvent) {
    if (!this.state.enabled) {
      return;
    }
    const flags = getEventDispositionFlags(e);
    if (this.direction === 'back') {
      this.browserProxy_.browserControlsHandler.back(flags);
    } else {
      this.browserProxy_.browserControlsHandler.forward(flags);
    }
    if (this.glowUpEnabled) {
      // The glow up animation should only trigger for physical mouse clicks.
      // - Keyboard interactions trigger a synthetic 'click' event (handled via
      // onClick_ -> onShortPress_).
      // - Touch interactions trigger 'pointerup' but with a pointerType of
      // 'touch' or 'pen'.
      // - We also exclude middle/right clicks (button !== 0) and touch UI mode.
      const isPointerUp = e.type === 'pointerup';
      const isMouse =
          isPointerUp && (e as PointerEvent).pointerType === 'mouse';
      const isLeftClick = e.button === 0;
      if (!this.touchUi && isMouse && isLeftClick) {
        if (this.animating_) {
          this.updateComplete.then(() => {
            playIconAnimation(this.$.button);
          });
        } else {
          this.animating_ = true;
          this.requestUpdate();
          this.playAnimation_();
        }
      }
    }
  }

  /**
   * Triggers the SMIL icon animation after the DOM has updated with the
   * animated SVG icon.
   */
  private async playAnimation_() {
    await this.updateComplete;
    await this.$.button.updateComplete;
    const crIcon = this.$.button.shadowRoot?.querySelector('cr-icon');
    if (crIcon && 'updateComplete' in crIcon) {
      await (crIcon as CrLitElement).updateComplete;
    }
    playIconAnimation(this.$.button);
    this.listenToAnimationEnd_();
  }

  /**
   * Listens to the SMIL animation end event on the animate element.
   * Resets the animating state once the animation finishes to fall back to the
   * static icon.
   */
  private listenToAnimationEnd_() {
    // Query for either 'animate' or 'animateTransform' elements that have
    // 'begin="indefinite"', which indicates they are triggerable animations.
    const animate =
        this.$.button.shadowRoot?.querySelector('cr-icon')?.shadowRoot?.querySelector(
            'animate[begin="indefinite"], animateTransform[begin="indefinite"]');
    if (!animate) {
      this.animating_ = false;
      this.requestUpdate();
      return;
    }
    const handler = () => {
      animate.removeEventListener('endEvent', handler);
      this.animating_ = false;
      this.requestUpdate();
    };
    animate.addEventListener('endEvent', handler);
  }

  protected getAriaLabel_(): string {
    return this.direction === 'back' ?
        loadTimeData.getString('backButtonAccName') :
        loadTimeData.getString('forwardButtonAccName');
  }

  protected getTooltip_(): string {
    return this.adjustTooltipForHelpBubble(
        this.direction === 'back' ?
            loadTimeData.getString('backButtonTooltip') :
            loadTimeData.getString('forwardButtonTooltip'));
  }

  // Note on keyboard accessibility: Since `<cr-icon-button>` is natively
  // focusable, keyboard navigation (Space/Enter) targets the button directly
  // rather than `#buttonWrapper`. Keyboard activations bypass `onPointerdown_`
  // and manual ripple triggers entirely, naturally activating native button
  // ripples and actions without interference (Fitts' law does not apply to
  // keyboard navigation).
  protected onPointerdown_(e: PointerEvent) {
    if (!this.state.enabled) {
      return;
    }
    this.pressHandler_.onPointerdown(e);
    // When clicking the expanded Fitts' law wrapper space outside the button,
    // manually start the button's ripple and set a tracking flag.
    if (!e.composedPath().includes(this.$.button)) {
      this.manualRippleTriggered_ = true;
      this.$.button.getRipple().uiDownAction(e);
    }
  }

  private clearManualRipple_() {
    // Only clean up the ripple if a manual down action was initiated earlier
    // (`manualRippleTriggered_`) by pressing the outer wrapper space.
    // By strictly checking this flag, we reliably clear the ripple even if
    // the pointer was subsequently dragged over onto `#button` before release,
    // while completely avoiding redundant double `uiUpAction()` executions if
    // both pointerleave and pointerup fire sequentially during drag-outs.
    if (this.manualRippleTriggered_) {
      this.manualRippleTriggered_ = false;
      this.$.button.getRipple().uiUpAction();
    }
  }

  protected onPointerup_(e: PointerEvent) {
    this.pressHandler_.onPointerup(e);
    this.clearManualRipple_();
  }

  protected onPointercancel_(e: PointerEvent) {
    this.pressHandler_.onPointercancel(e);
    this.clearManualRipple_();
  }

  protected onPointerleave_() {
    this.clearManualRipple_();
  }

  protected onPointerenter_() {
    if (!this.state.enabled) {
      return;
    }
    if (this.direction === 'back') {
      this.browserProxy_.browserControlsHandler.backButtonHovered();
    }
  }

  protected onClick_(e: MouseEvent) {
    // Only handle keyboard 'click', which triggers a left-click equivalent.
    // Other events like mouse 'click' are handled in onShortPress_.
    if (e.detail === 0) {
      this.onShortPress_(e);
    }
  }

  protected getIronIcon_(): string {
    if (this.glowUpEnabled && this.animating_) {
      return this.direction === 'back' ? 'webui-toolbar:back_arrow_glow_up' :
                                         'webui-toolbar:forward_arrow_glow_up';
    }

    if (this.direction === 'back') {
      if (roundedIconsEnabled()) {
        return 'webui-toolbar:arrow_back';
      } else {
        return this.touchUi ? 'webui-toolbar:back_arrow_touch_old' :
                              'webui-toolbar:back_arrow_chrome_refresh_old';
      }
    } else {
      if (roundedIconsEnabled()) {
        return 'webui-toolbar:arrow_forward';
      } else {
        return this.touchUi ? 'webui-toolbar:forward_arrow_touch_old' :
                              'webui-toolbar:forward_arrow_chrome_refresh_old';
      }
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'back-forward-button': BackForwardButtonElement;
  }
}

customElements.define(BackForwardButtonElement.is, BackForwardButtonElement);
