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

  constructor() {
    super();
    this.mojoInterfaceProvider = getAppParentalControlsProvider();
  }

  override ready(): void {
    super.ready();

    this.addEventListener('click', () => {
      this.updateBlockedState_(!this.app.isBlocked);
    });
  }

  private isAllowed_(app: App): boolean {
    return !app.isBlocked;
  }

  private onToggleChange_(e: CustomEvent<boolean>): void {
    const isBlocked = !e.detail;
    this.updateBlockedState_(isBlocked);
  }

  private updateBlockedState_(isBlocked: boolean): void {
    this.mojoInterfaceProvider.updateApp(this.app.id, isBlocked);
  }

  private getIconUrl_(app: App): string {
    // Use a no-op query param that reflects the app blocked state.
    // This ensures that the icon is fetched every time the state of the app is
    // updated. Otherwise, the icon is cached if the src stays the same.
    return `chrome://app-icon/${app.id}/64?` +
        `parental_controls_blocked=${app.isBlocked}`;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'block-app-item': BlockAppItemElement;
  }
}

customElements.define(BlockAppItemElement.is, BlockAppItemElement);
