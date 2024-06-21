// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_input/cr_input.js';
import '//resources/cr_elements/cr_drawer/cr_drawer.js';

import type {CrDrawerElement} from '//resources/cr_elements/cr_drawer/cr_drawer.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';

export interface RelatedWebsiteSetsAppElement {
  $: {
    drawer: CrDrawerElement,
  };
}

export class RelatedWebsiteSetsAppElement extends CrLitElement {
  static get is() {
    return 'related-website-sets-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      pageTitle_: {type: String},
      narrow_: {type: Boolean},
      isDrawerOpen_: {type: Boolean},
    };
  }

  protected pageTitle_: string = 'Related Website Sets';
  protected narrow_: boolean = true;
  protected isDrawerOpen_: boolean = false;

  protected handleNarrowChange(e: CustomEvent) {
    this.narrow_ = e.detail.data;
    if (this.$.drawer.open && !this.narrow_) {
      this.$.drawer.close();
    }
  }

  protected handleMenuButtonClick() {
    this.showDrawerMenu_();
  }

  private showDrawerMenu_() {
    this.$.drawer.openDrawer();
    this.isDrawerOpen_ = this.$.drawer.open;
  }
  protected onDrawerClose_() {
    this.isDrawerOpen_ = this.$.drawer.open;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'related-website-sets-app': RelatedWebsiteSetsAppElement;
  }
}

customElements.define(
    RelatedWebsiteSetsAppElement.is, RelatedWebsiteSetsAppElement);
