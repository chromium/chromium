// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './updater_state_card.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '../icons.html.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {FilePath} from '//resources/mojo/mojo/public/mojom/base/file_path.mojom-webui.js';

import {BrowserProxyImpl} from '../browser_proxy.js';
import type {GetUpdaterStatesResponse, UpdaterState} from '../updater_ui.mojom-webui.js';

import {getCss} from './updater_state.css.js';
import {getHtml} from './updater_state.html.js';

export class UpdaterStateElement extends CrLitElement {
  static get is() {
    return 'updater-state';
  }

  static override get styles() {
    return getCss();
  }

  static override get properties() {
    return {
      userUpdaterState: {type: Object},
      systemUpdaterState: {type: Object},
      error: {type: Boolean},
    };
  }

  protected accessor userUpdaterState: UpdaterState|null = null;
  protected accessor systemUpdaterState: UpdaterState|null = null;
  protected accessor error: boolean = false;

  override connectedCallback() {
    super.connectedCallback();
    this.getUpdaterStates()
        .then(response => {
          this.userUpdaterState = response.user;
          this.systemUpdaterState = response.system;
        })
        .catch(() => {
          this.error = true;
        });
  }

  override render() {
    return getHtml.bind(this)();
  }

  private async getUpdaterStates(): Promise<GetUpdaterStatesResponse> {
    return await BrowserProxyImpl.getInstance().handler.getUpdaterStates();
  }

  protected filePathToString(filePath: FilePath): string {
    if (typeof filePath.path === 'string') {
      return filePath.path;
    }

    const decoder = new TextDecoder('utf-16');
    const buffer = new Uint16Array(filePath.path);
    return decoder.decode(buffer);
  }

  protected shouldShowSystemUpdaterState():
      this is {systemUpdaterState: UpdaterState} {
    return !this.error && this.systemUpdaterState !== null;
  }

  protected shouldShowUserUpdaterState():
      this is {userUpdaterState: UpdaterState} {
    return !this.error && this.userUpdaterState !== null;
  }

  protected get shouldShowNoUpdatersFound(): boolean {
    return !this.error && this.systemUpdaterState === null &&
        this.userUpdaterState === null;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'updater-state': UpdaterStateElement;
  }
}

customElements.define(UpdaterStateElement.is, UpdaterStateElement);
