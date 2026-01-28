// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';

import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {Tab as TabData} from '/tab_strip_api/tab_strip_api_data_model.mojom-webui.js';
import {NetworkState} from '/tab_strip_api/tab_strip_api_data_model.mojom-webui.js';

import {getCss} from './tab.css.js';
import {getHtml} from './tab.html.js';

export class TabElement extends CrLitElement {
  static get is() {
    return 'webui-browser-tab';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      data: {type: Object},
      dragInProgress: {
        type: Boolean,
        reflect: true,
      },
      active: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  accessor data: TabData = {
    alertStates: [],
    favicon: {dataUrl: 'chrome://favicon2/'},
    id: '',
    isActive: false,
    isBlocked: false,
    isSelected: false,
    networkState: NetworkState.kNone,
    title: '',
    url: '',
  };

  protected accessor dragInProgress = false;
  protected accessor active = false;

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('data')) {
      this.active = this.data.isActive;
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('data')) {
      if (this.data.favicon.dataUrl) {
        this.style.setProperty(
            '--favicon-url', `url(${this.data.favicon.dataUrl})`);
      }
      this.style.setProperty('z-index', this.data.isActive ? '1' : '0');
    }
  }

  // Calculating the dom matrix value could be expensive.
  // This potentially could just be stored in the Tab and then updated manually
  // by the tabstrip during animation.
  getTransformX() {
    const matrix = new DOMMatrixReadOnly(this.style.transform);
    return matrix.m41;
  }

  protected onCloseClick() {
    this.fire('tab-close-click', {id: this.data.id});
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'webui-browser-tab': TabElement;
  }
}

customElements.define(TabElement.is, TabElement);
