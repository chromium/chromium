// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './extension_element.js';

import {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {assert} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {TrackedElementManager} from '//resources/js/tracked_element/tracked_element_manager.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {MenuSourceType} from '//resources/mojo/ui/base/mojom/menu_source_type.mojom-webui.js';

import {ExtensionElement} from './extension_element.js';
import {getCss} from './extensions_bar.css.js';
import type {ExtensionActionInfo} from './extensions_bar.mojom-webui.js';
import {PageCallbackRouter, PageHandlerFactory, PageHandlerRemote} from './extensions_bar.mojom-webui.js';

export class ExtensionsBar extends CrLitElement {
  static get is() {
    return 'webui-browser-extensions-bar';
  }

  static override get styles() {
    return getCss();
  }

  static override get properties() {
    return {
      visible: {type: Boolean, reflect: true},
    };
  }

  accessor visible: boolean = false;

  private callbackRouter: PageCallbackRouter = new PageCallbackRouter();
  private handler: PageHandlerRemote = new PageHandlerRemote();

  private buttons: Map<string, ExtensionElement> = new Map();
  private extensionsMenuButton: CrIconButtonElement;
  private trackedElementManager: TrackedElementManager;

  constructor() {
    super();
    this.trackedElementManager = TrackedElementManager.getInstance();
    this.extensionsMenuButton = new CrIconButtonElement();
    this.extensionsMenuButton.id = 'extensionsMenuButton';
    this.extensionsMenuButton.ironIcon =
        'webui-browser:ExtensionChromeRefreshIcon';
    this.extensionsMenuButton.title =
        loadTimeData.getString('tooltipExtensionsButton');
    this.extensionsMenuButton.addEventListener(
        'click', this.extensionMenuButtonClicked.bind(this));
    this.trackedElementManager.startTracking(
        this.extensionsMenuButton, 'kExtensionsMenuButtonElementId');

    const factory = PageHandlerFactory.getRemote();
    factory.createPageHandler(
        this.callbackRouter.$.bindNewPipeAndPassRemote(),
        this.handler.$.bindNewPipeAndPassReceiver());
    this.callbackRouter.actionsAddedOrUpdated.addListener(
        this.actionsAddedOrUpdated.bind(this));
    this.callbackRouter.actionRemoved.addListener(
        this.actionRemoved.bind(this));
    this.callbackRouter.actionPoppedOut.addListener(
        this.actionPoppedOut.bind(this));
  }

  // We manage the rendering directly.
  override render() {
    return '';
  }

  protected override firstUpdated() {
    this.shadowRoot.appendChild(this.extensionsMenuButton);
  }

  private actionsAddedOrUpdated(updates: ExtensionActionInfo[]) {
    for (const update of updates) {
      if (!this.buttons.has(update.id)) {
        const extensionButton = new ExtensionElement(update.id, this);
        this.buttons.set(update.id, extensionButton);
        this.shadowRoot.insertBefore(
            extensionButton, this.extensionsMenuButton);
      }

      const extensionButton = this.buttons.get(update.id);
      assert(extensionButton);
      extensionButton.iconUrl = update.dataUrlForIcon.url;
      extensionButton.setAttribute('aria-label', update.accessibleName);
      extensionButton.setAttribute('title', update.tooltip);
      extensionButton.visible = update.isVisible;

      extensionButton.requestUpdate();
    }
    this.updateVisibility();
  }

  private actionRemoved(actionId: string) {
    const extensionButton = this.buttons.get(actionId);
    assert(extensionButton);
    this.buttons.delete(actionId);
    extensionButton.remove();
    this.updateVisibility();
  }

  private actionPoppedOut() {
    // TODO(webium): If we have an animation (which we ought to); this should
    // probably consider its timing.
    return {};
  }

  private updateVisibility() {
    this.visible = (this.buttons.size !== 0);
  }

  onClick(id: string) {
    this.handler.executeUserAction(id);
  }

  onContextMenu(source: MenuSourceType, id: string) {
    this.handler.showContextMenu(source, id);
  }

  private extensionMenuButtonClicked() {
    this.handler.toggleExtensionsMenuFromWebUI();
  }

  /* Still TODO things:
   * Context menus.
   * Disabled icons.
   */
}

declare global {
  interface HTMLElementTagNameMap {
    'webui-browser-extensions-bar': ExtensionsBar;
  }
}

customElements.define(ExtensionsBar.is, ExtensionsBar);
