// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';

import {assertNotReached} from '//resources/js/assert.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import type {BrowserProxy} from './browser_proxy.js';
import {getCss} from './pinned_toolbar_action.css.js';
import {getHtml} from './pinned_toolbar_action.html.js';
import {PinnedToolbarAction} from './toolbar_ui_api_data_model.mojom-webui.js';
import type {PinnedToolbarActionState} from './toolbar_ui_api_data_model.mojom-webui.js';

export class PinnedToolbarActionElement extends CrLitElement {
  static get is() {
    return 'pinned-toolbar-action';
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

  accessor state: PinnedToolbarActionState = {
    action: PinnedToolbarAction.kUnspecified,
    highlighted: false,
    enabled: true,
    tooltip: '',
    accessibilityText: '',
  };

  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();

  protected getIcon_(): string {
    const type = this.state.action;

    // TODO(crbug.com/474061420): Fill this in.
    switch (type) {
      case PinnedToolbarAction.kUnspecified:
        assertNotReached();
      case PinnedToolbarAction.kShowPasswordsBubbleOrPage:
        return 'cr20:password';
      default:
        return 'cr:info';
    }
  }

  protected onActionClick_() {
    this.browserProxy_.toolbarUIHandler.invokePinnedToolbarAction(
        this.state.action);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'pinned-toolbar-action': PinnedToolbarActionElement;
  }
}

customElements.define(
    PinnedToolbarActionElement.is, PinnedToolbarActionElement);
