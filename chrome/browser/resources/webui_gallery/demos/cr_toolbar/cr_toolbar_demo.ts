// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_checkbox/cr_checkbox.js';
import '//resources/cr_elements/cr_input/cr_input.js';
import '//resources/cr_elements/cr_toolbar/cr_toolbar.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './cr_toolbar_demo.css.js';
import {getHtml} from './cr_toolbar_demo.html.js';

export class CrToolbarDemoElement extends CrLitElement {
  static get is() {
    return 'cr-toolbar-demo';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      alwaysShowLogo_: {type: Boolean},
      clearLabel_: {type: String},
      log_: {type: Array},
      menuLabel_: {type: String},
      narrow_: {type: Boolean},
      narrowThreshold_: {type: Number},
      pageName_: {type: String},
      searchPrompt_: {type: String},
      showMenu_: {type: Boolean},
      showSearch_: {type: Boolean},
      showSlottedContent_: {type: Boolean},
    };
  }

  protected accessor alwaysShowLogo_: boolean = true;
  protected accessor clearLabel_: string = 'Clear search';
  protected accessor log_: string[] = [];
  protected accessor menuLabel_: string = 'Menu';
  protected accessor narrow_: boolean|undefined;
  protected accessor narrowThreshold_: number = 1000;
  protected accessor pageName_: string = 'Demo';
  protected accessor searchPrompt_: string = 'Search through some content';
  protected accessor showMenu_: boolean = true;
  protected accessor showSearch_: boolean = true;
  protected accessor showSlottedContent_: boolean = false;

  protected onCrToolbarMenuClick_() {
    this.log_.push('Menu tapped.');
    this.requestUpdate();
  }

  protected onSearchChanged_(e: CustomEvent<string>) {
    this.log_.push(
        e.detail ? `Search term changed: ${e.detail}` : 'Search cleared.');
    this.requestUpdate();
  }

  protected onPageNameValueChanged_(e: CustomEvent<{value: string}>) {
    this.pageName_ = e.detail.value;
  }

  protected onSearchPromptValueChanged_(e: CustomEvent<{value: string}>) {
    this.searchPrompt_ = e.detail.value;
  }

  protected onClearLabelValueChanged_(e: CustomEvent<{value: string}>) {
    this.clearLabel_ = e.detail.value;
  }

  protected onNarrowThresholdValueChanged_(e: CustomEvent<{value: string}>) {
    this.narrowThreshold_ = Number(e.detail.value);
  }

  protected onMenuLabelValueChanged_(e: CustomEvent<{value: string}>) {
    this.menuLabel_ = e.detail.value;
  }

  protected onAlwaysShowLogoCheckedChanged_(e: CustomEvent<{value: boolean}>) {
    this.alwaysShowLogo_ = e.detail.value;
  }

  protected onShowMenuCheckedChanged_(e: CustomEvent<{value: boolean}>) {
    this.showMenu_ = e.detail.value;
  }

  protected onShowSearchCheckedChanged_(e: CustomEvent<{value: boolean}>) {
    this.showSearch_ = e.detail.value;
  }

  protected onShowSlottedContentCheckedChanged_(
      e: CustomEvent<{value: boolean}>) {
    this.showSlottedContent_ = e.detail.value;
  }

  protected onNarrowChanged_(e: CustomEvent<{value: boolean}>) {
    this.narrow_ = e.detail.value;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-toolbar-demo': CrToolbarDemoElement;
  }
}

export const tagName = CrToolbarDemoElement.is;

customElements.define(CrToolbarDemoElement.is, CrToolbarDemoElement);
