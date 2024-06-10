// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://customize-chrome-side-panel.top-chrome/shared/sp_heading.js';

import type {SpHeadingElement} from 'chrome://customize-chrome-side-panel.top-chrome/shared/sp_heading.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {Action, Category, CustomizeToolbarHandlerInterface} from '../customize_toolbar.mojom-webui.js';

import {CustomizeToolbarApiProxy} from './customize_toolbar_api_proxy.js';
import {getCss} from './toolbar.css.js';
import {getHtml} from './toolbar.html.js';

export interface ToolbarElement {
  $: {
    heading: SpHeadingElement,
  };
}

export class ToolbarElement extends CrLitElement {
  static get is() {
    return 'customize-chrome-toolbar';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      actions_: {type: Array},
      categories_: {type: Array},
    };
  }

  private handler_: CustomizeToolbarHandlerInterface;
  private listenerIds_: number[] = [];

  protected actions_: Action[] = [];
  protected categories_: Category[] = [];

  constructor() {
    super();
    this.handler_ = CustomizeToolbarApiProxy.getInstance().handler;

    this.handler_.listActions().then(({actions}) => {
      this.actions_ = actions;
    });

    this.handler_.listCategories().then(({categories}) => {
      this.categories_ = categories;
    });
  }

  override connectedCallback() {
    super.connectedCallback();
    const callbackRouter =
        CustomizeToolbarApiProxy.getInstance().callbackRouter;
    this.listenerIds_.push(callbackRouter.setActionPinned.addListener(
        this.setActionPinned_.bind(this)));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    const callbackRouter =
        CustomizeToolbarApiProxy.getInstance().callbackRouter;
    this.listenerIds_.forEach(id => callbackRouter.removeListener(id));
    this.listenerIds_ = [];
  }

  focusOnBackButton() {
    this.$.heading.getBackButton().focus();
  }

  protected onBackClick_() {
    this.fire('back-click');
  }

  protected getActionToggleHandler_(actionId: number) {
    return (event: CustomEvent<boolean>) =>
               this.handler_.pinAction(actionId, event.detail);
  }

  private setActionPinned_(actionId: number, pinned: boolean) {
    this.actions_ = this.actions_.map((action) => {
      if (action.id === actionId) {
        action.pinned = pinned;
      }

      return action;
    });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'customize-chrome-toolbar': ToolbarElement;
  }
}

customElements.define(ToolbarElement.is, ToolbarElement);
