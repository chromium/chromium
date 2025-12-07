// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_slider/cr_slider.js';

import {CrContainerShadowMixinLit} from '//resources/cr_elements/cr_container_shadow_mixin_lit.js';
import type {CrSliderElement} from '//resources/cr_elements/cr_slider/cr_slider.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './scroll_view_demo.css.js';
import {getHtml} from './scroll_view_demo.html.js';

export interface ScrollViewDemoElement {
  $: {
    itemsLengthSlider: CrSliderElement,
  };
}

const ScrollViewDemoElementBase = CrContainerShadowMixinLit(CrLitElement);

export class ScrollViewDemoElement extends ScrollViewDemoElementBase {
  static get is() {
    return 'scroll-view-demo';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      items_: {type: Array},
    };
  }

  protected accessor items_: number[] = [0, 1, 2, 3];

  protected onItemsLengthChanged_() {
    const length = this.$.itemsLengthSlider.value;
    const items: number[] = [];
    for (let i = 0; i < length; i++) {
      items.push(i);
    }
    this.items_ = items;
  }
}

export const tagName = ScrollViewDemoElement.is;

customElements.define(ScrollViewDemoElement.is, ScrollViewDemoElement);
