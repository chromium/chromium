// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** Base template with elements common to all Cellular Setup flow sub-pages. */
import './cellular_setup_icons.html.js';
import '//resources/ash/common/cr_elements/cros_color_overrides.css.js';
import '//resources/ash/common/cr_elements/cr_shared_vars.css.js';
import '//resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './base_page.html.js';

export class BasePageElement extends PolymerElement {
  static get is() {
    return 'base-page' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Main title for the page.
       */
      title: String,

      /**
       * Message displayed under the main title.
       */
      message: String,

      /**
       * Name for the cellular-setup iconset iron-icon displayed beside message.
       */
      messageIcon: {
        type: String,
        value: '',
      },
    };
  }

  override title: string;
  message: string;
  messageIcon: string;

  private getTitle_(): string {
    return this.title;
  }

  private isTitleShown_(): boolean {
    return !!this.title;
  }

  private isMessageIconShown_(): boolean {
    return !!this.messageIcon;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [BasePageElement.is]: BasePageElement;
  }
}

customElements.define(BasePageElement.is, BasePageElement);
