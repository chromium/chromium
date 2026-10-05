// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '/shared/icon_from_table.js';

import type {CrButtonElement} from '//resources/cr_elements/cr_button/cr_button.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import type {ExtensionActionInfo} from '/shared/extensions_bar_data_model.mojom-webui.js';
import {getContextMenuSourceType, setHasHelpBubble} from '/shared/toolbar_button.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import {getHtml} from './extension.html.js';
import {ToolbarActionMixin} from './toolbar_action_mixin.js';
import {getCss} from './toolbar_button.css.js';

const initialState: ExtensionActionInfo = {
  id: '',
  accessibleName: '',
  tooltip: '',
  isVisible: false,
  icon: {handleId: 0n},
  isPinnedByDefaultIphAnchor: false,
};

// Help bubble anchor for the "extension pinned by default" IPH. Registered in
// addition to the primary element ID, only while the browser requests it.
const PINNED_BY_DEFAULT_IPH_ELEMENT_ID = 'kExtensionsPinnedByDefaultElementId';

const ExtensionElementBase = ToolbarActionMixin(CrLitElement, initialState);

export interface ExtensionElement {
  $: {
    button: CrButtonElement,
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
