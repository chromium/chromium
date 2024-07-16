// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './toggle_row.js';

import {assert, assertNotReached} from '//resources/js/assert.js';
import type {App} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {BrowserProxy} from 'chrome://resources/cr_components/app_management/browser_proxy.js';
import {AppManagementUserAction, WindowMode} from 'chrome://resources/cr_components/app_management/constants.js';
import {recordAppManagementUserAction} from 'chrome://resources/cr_components/app_management/util.js';
import {CrLitElement, type PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app_management_shared_style.css.js';
import type {ToggleRowElement} from './toggle_row.js';
import {createDummyApp} from './web_app_settings_utils.js';
import {getHtml} from './window_mode_item.html.js';

function convertWindowModeToBool(windowMode: WindowMode): boolean {
  switch (windowMode) {
    case WindowMode.kBrowser:
      return false;
    case WindowMode.kWindow:
      return true;
    default:
      assertNotReached();
  }
}

function getWindowModeBoolean(windowMode: WindowMode): boolean {
  assert(windowMode !== WindowMode.kUnknown, 'Window Mode Not Set');
  return convertWindowModeToBool(windowMode);
}

export class WindowModeItemElement extends CrLitElement {
  static get is() {
    return 'app-management-window-mode-item';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      windowModeLabel: {type: String},

      app: {type: Object},

      hidden: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  windowModeLabel: string = '';
  app: App = createDummyApp();
  override hidden: boolean = false;

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('app')) {
      this.hidden = this.isHidden_();
    }
  }

  override firstUpdated() {
    this.addEventListener('click', this.onClick_);
    this.addEventListener('change', this.toggleWindowMode_);
  }

  protected getValue_(): boolean {
    return getWindowModeBoolean(this.app.windowMode);
  }

  private onClick_() {
    this.shadowRoot!.querySelector<ToggleRowElement>('#toggle-row')!.click();
  }

  private toggleWindowMode_() {
    const currentWindowMode = this.app.windowMode;
    if (currentWindowMode === WindowMode.kUnknown) {
      assertNotReached();
    }
    const newWindowMode = (currentWindowMode === WindowMode.kBrowser) ?
        WindowMode.kWindow :
        WindowMode.kBrowser;
    BrowserProxy.getInstance().handler.setWindowMode(
        this.app.id,
        newWindowMode,
    );
    const booleanWindowMode = getWindowModeBoolean(newWindowMode);
    const windowModeChangeAction = booleanWindowMode ?
        AppManagementUserAction.WINDOW_MODE_CHANGED_TO_WINDOW :
        AppManagementUserAction.WINDOW_MODE_CHANGED_TO_BROWSER;
    recordAppManagementUserAction(this.app.type, windowModeChangeAction);
  }

  private isHidden_(): boolean {
    return this.app.hideWindowMode;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'app-management-window-mode-item': WindowModeItemElement;
  }
}

customElements.define(WindowModeItemElement.is, WindowModeItemElement);
