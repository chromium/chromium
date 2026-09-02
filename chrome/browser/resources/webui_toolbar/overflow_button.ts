// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/icons.html.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {OverflowMenuItem} from '/shared/toolbar_ui_api.mojom-webui.js';
import type {OverflowButtonControlState} from '/shared/toolbar_ui_api_data_model.mojom-webui.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import {getHtml} from './overflow_button.html.js';
import {getCss} from './toolbar_button.css.js';
import {BUTTON_LEFT, getClickSourceType, getContextMenuPosition, HelpBubbleAnchorMixin} from './toolbar_button.js';

const OverflowButtonElementBase = HelpBubbleAnchorMixin(CrLitElement);

export class OverflowButtonElement extends OverflowButtonElementBase {
  static get is() {
    return 'overflow-button';
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
      getOverflowedMenuItems: {type: Object},
    };
  }

  accessor state: OverflowButtonControlState = {
    isContextMenuVisible: false,
  };

  accessor getOverflowedMenuItems: () => OverflowMenuItem[] = () => [];

  protected getLabel_(): string {
    return loadTimeData.getString('overflowButtonTooltip');
  }
  protected getTooltip_(): string {
    return this.adjustTooltipForHelpBubble(
        loadTimeData.getString('overflowButtonTooltip'));
  }

  protected onPointerdown_(e: PointerEvent) {
    // To match Views' MenuButtonController behavior:
    // Mouse events trigger the menu immediately on press.
    // Touch/Gesture events are ignored on press and instead trigger on tap
    // (release).
    if (e.pointerType !== 'mouse' || e.button !== BUTTON_LEFT) {
      return;
    }
    this.handleMenuClick_(e);
  }

  protected onClick_(e: PointerEvent) {
    // Handle keyboard (detail === 0) and touch clicks (tap/release) here.
    // Mouse clicks (detail > 0) are ignored here because they were already
    // handled immediately on pointerdown to match Views' behavior.
    if (e.detail === 0 || e.pointerType !== 'mouse') {
      this.handleMenuClick_(e);
    }
  }

  // Shows a popup menu containing menu items corresponding to any overflowed
  // controls.
  private handleMenuClick_(e: Event) {
    BrowserProxyImpl.getInstance().toolbarUIHandler.showOverflowMenu(
        this.getOverflowedMenuItems(), getContextMenuPosition(this),
        getClickSourceType(e));
  }
}

customElements.define(OverflowButtonElement.is, OverflowButtonElement);

declare global {
  interface HTMLElementTagNameMap {
    'overflow-button': OverflowButtonElement;
  }
}
