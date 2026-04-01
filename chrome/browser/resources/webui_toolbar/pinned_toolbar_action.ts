// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';

import {assertNotReached} from '//resources/js/assert.js';
import {TrackedElementManager} from '//resources/js/tracked_element/tracked_element_manager.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

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
    elementId: null,
  };

  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();
  private trackedElementManager_: TrackedElementManager =
      TrackedElementManager.getInstance();

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.trackedElementManager_.stopTracking(this);
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('state')) {
      const oldState = changedProperties.get('state');
      const oldId = oldState?.elementId;
      const newId = this.state.elementId;

      if (oldId !== newId) {
        if (oldId) {
          this.trackedElementManager_.stopTracking(this);
        }
        if (newId) {
          this.trackedElementManager_.startTracking(this, newId);
        }
      }
    }
  }

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
