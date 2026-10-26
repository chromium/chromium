// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './toolbar_chip_button.js';
import '/shared/icon_from_table.js';

import {TrackedElementManager} from '//resources/js/tracked_element/tracked_element_manager.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import type {ExtensionActionInfo} from '/shared/extensions_bar_data_model.mojom-webui.js';
import {getContextMenuSourceType, setHasHelpBubble} from '/shared/toolbar_button.js';
import type {OverflowMenuItem} from '/shared/toolbar_ui_api.mojom-webui.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import {getCss} from './extension.css.js';
import {getHtml} from './extension.html.js';
import {OverflowableToolbarActionMixin} from './overflowable_toolbar_action_mixin.js';
import {ToolbarActionMixin} from './toolbar_action_mixin.js';
import type {ToolbarChipButtonElement} from './toolbar_chip_button.js';

const initialState: ExtensionActionInfo = {
  id: '',
  accessibleName: '',
  tooltip: '',
  isVisible: false,
  forceVisible: false,
  icon: {handleId: 0n},
  isPinnedByDefaultIphAnchor: false,
  highlighted: false,
};

// Help bubble anchor for the "extension pinned by default" IPH. Registered in
// addition to the primary element ID, only while the browser requests it.
const PINNED_BY_DEFAULT_IPH_ELEMENT_ID = 'kExtensionsPinnedByDefaultElementId';

const ExtensionElementBase = OverflowableToolbarActionMixin(
    ToolbarActionMixin(CrLitElement, initialState));

export interface ExtensionElement {
  $: {
    button: ToolbarChipButtonElement,
  };
}

export class ExtensionElement extends ExtensionElementBase {
  static get is() {
    return 'webui-toolbar-extension';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  private browserProxy_ = BrowserProxyImpl.getInstance();
  private isPinnedByDefaultIphAnchorRegistered_: boolean = false;

  override connectedCallback() {
    super.connectedCallback();
    // On the first connection, updated() registers the anchor after the initial
    // render. On later reconnections (e.g. after being moved in the DOM), there
    // may be no update, so re-register here.
    if (this.hasUpdated) {
      this.updatePinnedByDefaultIphAnchorRegistration_();
    }
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    // HelpBubbleMixin unregisters all help bubbles on disconnect.
    this.isPinnedByDefaultIphAnchorRegistered_ = false;
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('state') ||
        changedProperties.has('trackedHighlighted')) {
      // Actions the browser forces to be visible (see
      // ExtensionActionInfo::force_visible), e.g., the extensions menu button,
      // popped out actions, the pinned-by-default IPH anchor, and actions with
      // open context menus or anchored bubbles, must not be hidden due to
      // overflow, since the browser may be waiting for them to be shown so it
      // can anchor something to them. Highlighted actions (e.g., for an IPH
      // anchored to the button itself) also shouldn't be hidden.
      this.preventOverflow = this.state.forceVisible || this.trackedHighlighted;
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);
    if (changedProperties.has('state')) {
      this.updatePinnedByDefaultIphAnchorRegistration_();
    }
    if (changedProperties.has('hasHelpBubble')) {
      // The pulsing highlight in help_bubble_anchor.css is keyed off of the
      // host having the help-anchor-highlight class. HelpBubbleController only
      // adds that class to the anchor element, which for the pinned-by-default
      // IPH is the inner button, so mirror it onto the host.
      this.classList.toggle('help-anchor-highlight', this.hasHelpBubble);
    }
  }

  private updatePinnedByDefaultIphAnchorRegistration_() {
    const shouldRegister =
        this.isConnected && this.state.isPinnedByDefaultIphAnchor;
    if (shouldRegister === this.isPinnedByDefaultIphAnchorRegistered_) {
      return;
    }
    if (shouldRegister) {
      // TrackedElementManager tracks a single identifier per element, and this
      // element is already tracked as kToolbarActionViewElementId, so anchor to
      // the inner button instead.
      this.registerHelpBubble(PINNED_BY_DEFAULT_IPH_ELEMENT_ID, this.$.button, {
        secondaryId: this.getSecondaryElementId(),
        onHelpBubbleShown: () => setHasHelpBubble(this, true),
        onHelpBubbleHidden: () => setHasHelpBubble(this, false),
      });
    } else {
      this.unregisterHelpBubble(PINNED_BY_DEFAULT_IPH_ELEMENT_ID);
    }
    this.isPinnedByDefaultIphAnchorRegistered_ = shouldRegister;
  }

  // Extensions are hidden on overflow, but not added to the overflow menu.
  override getOverflowMenuItem(): OverflowMenuItem|null {
    return null;
  }

  override getElementId(state: ExtensionActionInfo): string {
    return state.id === '' ? 'kExtensionsMenuButtonElementId' :
                             'kToolbarActionViewElementId';
  }

  override getSecondaryElementId(): string {
    return 'ext:' + this.state.id;
  }

  protected onClick_(e: PointerEvent) {
    if (this.highlightTracker.shouldSkipClick(e)) {
      return;
    }
    TrackedElementManager.getInstance().notifyElementActivated(this);
    this.browserProxy_.toolbarUIHandler.executeExtensionAction(this.state.id);
  }

  protected onContextmenu_(e: Event) {
    e.preventDefault();
    this.browserProxy_.toolbarUIHandler.showExtensionContextMenu(
        this.state.id, getContextMenuSourceType(e));
  }

  override getMimeType() {
    return 'application/x-webui-extension-action';
  }

  override getItemId() {
    return this.state.id;
  }

  override isDraggable() {
    return this.state.id !== '';
  }

  override moveItemBy(delta: number) {
    this.browserProxy_.toolbarUIHandler.moveExtensionActionBy(
        this.state.id, delta);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'webui-toolbar-extension': ExtensionElement;
  }
}

customElements.define(ExtensionElement.is, ExtensionElement);
