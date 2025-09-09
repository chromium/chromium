// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './extension_element.js';

import {assert} from '//resources/js/assert.js';
import {CrLitElement, css} from '//resources/lit/v3_0/lit.rollup.js';

import {ExtensionElement} from './extension_element.js';
import type {ExtensionActionInfo} from './extensions_bar.mojom-webui.js';
import {PageCallbackRouter, PageHandlerFactory, PageHandlerRemote} from './extensions_bar.mojom-webui.js';

export class ExtensionsBar extends CrLitElement {
  static get is() {
    return 'webui-browser-extensions-bar';
  }

  static override get styles() {
    return css`:host { display: flex; }`;
  }

  private callbackRouter: PageCallbackRouter = new PageCallbackRouter();
  private handler: PageHandlerRemote = new PageHandlerRemote();

  private buttons: Map<string, ExtensionElement> = new Map();

  constructor() {
    super();
    const factory = PageHandlerFactory.getRemote();
    factory.createPageHandler(
        this.callbackRouter.$.bindNewPipeAndPassRemote(),
        this.handler.$.bindNewPipeAndPassReceiver());
    this.callbackRouter.actionsAddedOrUpdated.addListener(
        this.actionsAddedOrUpdated.bind(this));
    this.callbackRouter.actionRemoved.addListener(
        this.actionRemoved.bind(this));
  }

  // We manage the rendering directly.
  override render() {
    return '';
  }

  private actionsAddedOrUpdated(updates: ExtensionActionInfo[]) {
    for (const update of updates) {
      if (!this.buttons.has(update.id)) {
        const extensionButton = new ExtensionElement(update.id, this);
        this.buttons.set(update.id, extensionButton);
        this.shadowRoot.appendChild(extensionButton);
      }

      const extensionButton = this.buttons.get(update.id);
      assert(extensionButton);
      extensionButton.iconUrl = update.dataUrlForIcon.url;
      extensionButton.setAttribute('aria-label', update.accessibleName);

      extensionButton.requestUpdate();
    }
  }

  private actionRemoved(actionId: string) {
    const extensionButton = this.buttons.get(actionId);
    assert(extensionButton);
    this.buttons.delete(actionId);
    extensionButton.remove();
  }

  onClick(id: string) {
    this.handler.executeUserAction(id);
  }

  /* Initial TODO things:
   * Extensions button, for menu
   * Hiding things that are not pinned or otherwise visible, ref.
   *      ExtensionsToolbarContainer::IsActionVisibleOnToolbar
   * Disabled icons.
   */
}

declare global {
  interface HTMLElementTagNameMap {
    'webui-browser-extensions-bar': ExtensionsBar;
  }
}

customElements.define(ExtensionsBar.is, ExtensionsBar);
