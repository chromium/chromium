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

  protected alwaysShowLogo_: boolean = true;
  protected clearLabel_: string = 'Clear search';
  protected log_: string[] = [];
  protected menuLabel_: string = 'Menu';
  protected narrow_?: boolean;
  protected narrowThreshold_: number = 1000;
  protected pageName_: string = 'Demo';
  protected searchPrompt_: string = 'Search through some content';
  protected showMenu_: boolean = true;
  protected showSearch_: boolean = true;
  protected showSlottedContent_: boolean = false;

  protected onMenuClick_() {
    this.log_.push('Menu tapped.');
    this.requestUpdate();
  }

  protected onSearchChanged_(e: CustomEvent<string>) {
    this.log_.push(
        e.detail ? `Search term changed: ${e.detail}` : 'Search cleared.');
    this.requestUpdate();
  }

  protected onPageNameChanged_(e: CustomEvent<{value: string}>) {
    this.pageName_ = e.detail.value;
  }

  protected onSearchPromptChanged_(e: CustomEvent<{value: string}>) {
    this.searchPrompt_ = e.detail.value;
  }

  protected onClearLabelChanged_(e: CustomEvent<{value: string}>) {
    this.clearLabel_ = e.detail.value;
  }

  protected onNarrowThresholdChanged_(e: CustomEvent<{value: string}>) {
    this.narrowThreshold_ = Number(e.detail.value);
  }

  protected onMenuLabelChanged_(e: CustomEvent<{value: string}>) {
    this.menuLabel_ = e.detail.value;
  }

  protected onAlwaysShowLogoChanged_(e: CustomEvent<{value: boolean}>) {
    this.alwaysShowLogo_ = e.detail.value;
  }

  protected onShowMenuChanged_(e: CustomEvent<{value: boolean}>) {
    this.showMenu_ = e.detail.value;
  }

  protected onShowSearchChanged_(e: CustomEvent<{value: boolean}>) {
    this.showSearch_ = e.detail.value;
  }

  protected onShowSlottedContentChanged_(e: CustomEvent<{value: boolean}>) {
    this.showSlottedContent_ = e.detail.value;
  }

  protected onNarrowChanged_(e: CustomEvent<{value: boolean}>) {
    this.narrow_ = e.detail.value;
  }
}

export const tagName = CrToolbarDemoElement.is;

customElements.define(CrToolbarDemoElement.is, CrToolbarDemoElement);
