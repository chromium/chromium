// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './toolbar_chip_button.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import './icons.js';
import '//resources/cr_elements/icons.html.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {AppMenuIconType, AppMenuSeverity, ContextMenuType, FocusRequestTarget} from '/shared/toolbar_ui_api_data_model.mojom-webui.js';
import type {AppMenuControlState} from '/shared/toolbar_ui_api_data_model.mojom-webui.js';

import {getCss} from './app_menu_button.css.js';
import {getHtml} from './app_menu_button.html.js';
import {BrowserProxyImpl, INVALID_FOCUS_REQUEST_HANDLE} from './browser_proxy.js';
import type {FocusRequestHandle} from './browser_proxy.js';
import {BUTTON_LEFT, getClickSourceType, getContextMenuPosition, HelpBubbleAnchorMixin, setHasHelpBubble} from './toolbar_button.js';
import type {ToolbarChipButtonElement} from './toolbar_chip_button.js';

const AppMenuButtonElementBase = HelpBubbleAnchorMixin(CrLitElement);

export interface AppMenuButtonElement {
  $: {
    button: ToolbarChipButtonElement,
  };
}

export class AppMenuButtonElement extends AppMenuButtonElementBase {
  static get is() {
    return 'app-menu-button';
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
    };
  }

  accessor state: AppMenuControlState = {
    // Note: iconType is used by the backend to determine strings/labels
    // The frontend icon glyph remains a constant three-dot menu.
    iconType: AppMenuIconType.kNone,
    severity: AppMenuSeverity.kNone,
    labelText: null,
    accessibilityText: '',
    tooltip: '',
    isContextMenuVisible: false,
    windowIsMaximizedOrFullscreen: false,
  };

  private browserProxy_ = BrowserProxyImpl.getInstance();
  private focusRequestHandle_: FocusRequestHandle =
      INVALID_FOCUS_REQUEST_HANDLE;

  // Manage the lifecycle of the focus listener.
  override connectedCallback() {
    super.connectedCallback();
    this.registerHelpBubble('kToolbarAppMenuButtonElementId', this.$.button, {
      onHighlightChanged: (highlighted: boolean) => {
        this.classList.toggle('anchor-highlight', highlighted);
      },
      onHelpBubbleShown: () => setHasHelpBubble(this, true),
      onHelpBubbleHidden: () => setHasHelpBubble(this, false),
    });
    this.focusRequestHandle_ = this.browserProxy_.addFocusRequestListener(
        this.onFocusRequest_.bind(this));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.unregisterHelpBubble('kToolbarAppMenuButtonElementId');
    this.browserProxy_.removeFocusRequestListener(this.focusRequestHandle_);
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);
    if (changedProperties.has('state')) {
      this.toggleAttribute(
          'window-is-maximized-or-fullscreen',
          this.state.windowIsMaximizedOrFullscreen);
    }
  }

  override focus() {
    this.$.button.focus();
  }

  // Handles focus requests from the C++ side.
  private onFocusRequest_(target: FocusRequestTarget) {
    if (target === FocusRequestTarget.kAppMenu) {
      this.focus();
    }
  }

  // Reports focus changes to the C++ side.
  protected onFocusin_() {
    this.browserProxy_.toolbarUIHandler.onAppMenuFocusChanged(true);
  }

  protected onFocusout_() {
    this.browserProxy_.toolbarUIHandler.onAppMenuFocusChanged(false);
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

  private handleMenuClick_(e: Event) {
    this.browserProxy_.toolbarUIHandler.showContextMenu(
        ContextMenuType.kAppMenu, getContextMenuPosition(this),
        getClickSourceType(e), /*showMenuToken=*/ null);
  }

  protected getTooltip_(): string {
    return this.adjustTooltipForHelpBubble(this.state.tooltip);
  }

  protected getHighlightClass_(): string {
    const classes = [];
    if (this.hasHelpBubble) {
      classes.push('help-anchor-highlight');
    }
    if (this.state.severity !== AppMenuSeverity.kNone) {
      classes.push('has-severity');
    }
    return classes.join(' ');
  }
}

customElements.define(AppMenuButtonElement.is, AppMenuButtonElement);

declare global {
  interface HTMLElementTagNameMap {
    'app-menu-button': AppMenuButtonElement;
  }
}
