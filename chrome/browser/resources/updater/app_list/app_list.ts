// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../scope_icon.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import type {AppState} from '../updater_ui.mojom-webui.js';

import {getCss} from './app_list.css.js';
import {getHtml} from './app_list.html.js';

export interface AppStateDisplay extends AppState {
  scope: 'SYSTEM'|'USER';
  displayName: string;
}

export class AppListElement extends CrLitElement {
  static get is() {
    return 'app-list';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      apps: {type: Array},
      error: {type: Boolean},
    };
  }

  accessor apps: AppStateDisplay[] = [];
  accessor error: boolean = false;

  protected get shouldShowNoAppsMessage(): boolean {
    return !this.error && this.apps.length === 0;
  }

  protected get shouldShowTable(): boolean {
    return !this.error && this.apps.length !== 0;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'app-list': AppListElement;
  }
}

customElements.define(AppListElement.is, AppListElement);
