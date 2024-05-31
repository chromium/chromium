// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../../settings_shared.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {App, AppParentalControlsHandlerInterface} from '../../mojom-webui/app_parental_controls_handler.mojom-webui.js';

import {getTemplate} from './block_app_item.html.js';
import {getAppParentalControlsProvider} from './mojo_interface_provider.js';

/** An element that represents the app that can be blocked with the toggle. */
export class BlockAppItemElement extends PolymerElement {
  static get is() {
    return 'block-app-item';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      app: Object,
    };
  }

  app: App;
  private mojoInterfaceProvider: AppParentalControlsHandlerInterface;
  private iconVersionCounter: number = 0;

  constructor() {
    super();
    this.mojoInterfaceProvider = getAppParentalControlsProvider();
  }

  override connectedCallback(): void {
    super.connectedCallback();
  }

  private isAllowed_(app: App): boolean {
    return !app.isBlocked;
  }

  private onToggleChange_(e: CustomEvent<boolean>): void {
    const isBlocked = !e.detail;
    this.mojoInterfaceProvider.updateApp(this.app.id, isBlocked);
  }

  private getIconUrl_(app: App): string {
    // Use a no-op query param that is incremented when the app has an update.
    // This ensures that the icon is fetched every time the state of the app is
    // updated. Otherwise, the icon is cached if the src stays the same.
    return `chrome://app-icon/${app.id}/64?` +
        `parental_controls_version=${this.getIconVersion_()}`;
  }

  private getIconVersion_(): number {
    if (this.iconVersionCounter + 1 === Number.MAX_SAFE_INTEGER) {
      this.iconVersionCounter = 0;
    } else {
      this.iconVersionCounter++;
    }
    return this.iconVersionCounter;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'block-app-item': BlockAppItemElement;
  }
}

customElements.define(BlockAppItemElement.is, BlockAppItemElement);
