// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/icons.html.js';

import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {MenuSourceType} from '//resources/mojo/ui/base/mojom/menu_source_type.mojom-webui.js';

import {getCss} from './back_forward_button.css.js';
import {getHtml} from './back_forward_button.html.js';
import {BrowserProxyImpl, ContextMenuType} from './browser_proxy.js';
import type {BackForwardButtonState, BrowserProxy} from './browser_proxy.js';
import {OverflowableButtonMixin} from './overflowable_button.js';
import {getContextMenuPosition, getEventDispositionFlags, HelpBubbleAnchorMixin, PressHandler, roundedIconsEnabled} from './toolbar_button.js';

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
      leadingMargin: {type: Number},
      touchUi: {type: Boolean},
    };
  }

  accessor direction: 'back'|'forward' = 'back';
  override accessor state: BackForwardButtonState = {
    enabled: false,
    shouldBeShown: true,
    isContextMenuVisible: false,
  };
  accessor leadingMargin: number = 0;
  accessor touchUi: boolean = false;

  private manualRippleTriggered_: boolean = false;
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
        this.contextMenuType_, getContextMenuPosition(this), source);
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
