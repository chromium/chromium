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
      app: {
        type: Object,
      },

      // Checked toggle indicates that an app is allowed, unchecked blocked.
      toggleChecked_: {
        type: Boolean,
        value: true,
      },
    };
  }

  app: App;
  private toggleChecked_: boolean;
  private mojoInterfaceProvider: AppParentalControlsHandlerInterface;

  constructor() {
    super();
    this.mojoInterfaceProvider = getAppParentalControlsProvider();
  }

  private isAllowed_(app: App): boolean {
    return !app.isBlocked;
  }

  private onToggleChange_(e: CustomEvent<boolean>): void {
    this.toggleChecked_ = e.detail;
    this.mojoInterfaceProvider.updateApp(this.app.id, !this.toggleChecked_);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'block-app-item': BlockAppItemElement;
  }
}

customElements.define(BlockAppItemElement.is, BlockAppItemElement);
