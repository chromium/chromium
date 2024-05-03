// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://customize-chrome-side-panel.top-chrome/shared/sp_heading.js';
import 'chrome://customize-chrome-side-panel.top-chrome/shared/sp_shared_style.css.js';

import type {SpHeadingElement} from 'chrome://customize-chrome-side-panel.top-chrome/shared/sp_heading.js';
import type {CrToggleElement} from '//resources/cr_elements/cr_toggle/cr_toggle.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {CustomizeToolbarHandlerInterface} from '../customize_toolbar.mojom-webui.js';

import {CustomizeToolbarApiProxy} from './customize_toolbar_api_proxy.js';
import {getTemplate} from './toolbar.html.js';

export interface ToolbarElement {
  $: {
    heading: SpHeadingElement,
    actionToggle: CrToggleElement,
    actionLabel: HTMLHeadingElement,
  };
}

export class ToolbarElement extends PolymerElement {
  static get is() {
    return 'customize-chrome-toolbar';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {};
  }

  private handler_: CustomizeToolbarHandlerInterface;
  private listenerIds_: number[] = [];

  private actionId_: number;

  constructor() {
    super();
    this.handler_ = CustomizeToolbarApiProxy.getInstance().handler;

    this.handler_.listActions().then(({actions}) => {
      this.actionId_ = actions[0].id;
      this.$.actionLabel.innerText = actions[0].displayName;

      this.handler_.getActionPinned(this.actionId_).then(({pinned}) => {
        this.$.actionToggle.checked = pinned;
      });
    });
  }

  override connectedCallback() {
    super.connectedCallback();
    const callbackRouter =
        CustomizeToolbarApiProxy.getInstance().callbackRouter;
    this.listenerIds_.push(callbackRouter.setActionPinned.addListener(
        this.setActionPinned.bind(this)));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    const callbackRouter =
        CustomizeToolbarApiProxy.getInstance().callbackRouter;
    this.listenerIds_.forEach(id => callbackRouter.removeListener(id));
  }

  focusOnBackButton() {
    this.$.heading.getBackButton().focus();
  }

  private onBackClick_() {
    this.dispatchEvent(new Event('back-click'));
  }

  private onActionToggle_(event: CustomEvent) {
    this.handler_.pinAction(this.actionId_, event.detail);
  }

  private setActionPinned(actionId: number, pinned: boolean) {
    if (actionId !== this.actionId_) {
      return;
    }

    this.$.actionToggle.checked = pinned;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'customize-chrome-toolbar': ToolbarElement;
  }
}

customElements.define(ToolbarElement.is, ToolbarElement);
