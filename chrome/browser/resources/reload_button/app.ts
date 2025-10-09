// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/icons.html.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';

export class ReloadButtonAppElement extends CrLitElement {
  static get is() {
    return 'reload-button-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      reloadOrStopIcon_: {state: true, type: String},
    };
  }

  protected accessor reloadOrStopIcon_: string = 'icon-refresh';

  protected setReloadStopState(isLoading: boolean) {
    this.reloadOrStopIcon_ = isLoading ? 'icon-clear' : 'icon-refresh';
  }

  // TODO(crbug.com/444358999): implement the reload logic
  protected onReloadOrStopClick_(_: Event) {
    this.reloadOrStopIcon_ = this.reloadOrStopIcon_ === 'icon-refresh' ?
        'icon-clear' :
        'icon-refresh';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'reload-button-app': ReloadButtonAppElement;
  }
}

customElements.define(ReloadButtonAppElement.is, ReloadButtonAppElement);
