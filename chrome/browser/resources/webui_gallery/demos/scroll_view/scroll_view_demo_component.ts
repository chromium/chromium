// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import 'chrome://resources/cr_elements/cr_slider/cr_slider.js';

import {CrContainerShadowMixin} from 'chrome://resources/cr_elements/cr_container_shadow_mixin.js';
import {CrScrollableMixin} from 'chrome://resources/cr_elements/cr_scrollable_mixin.js';
import {CrSliderElement} from 'chrome://resources/cr_elements/cr_slider/cr_slider.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './scroll_view_demo_component.html.js';

interface ScrollViewDemoComponent {
  $: {
    itemsLengthSlider: CrSliderElement,
  };
}

const ScrollViewDemoComponentBase =
    CrContainerShadowMixin(CrScrollableMixin(PolymerElement));

class ScrollViewDemoComponent extends ScrollViewDemoComponentBase {
  static get is() {
    return 'scroll-view-demo';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      items_: {
        type: Array,
        value: () => [0, 1, 2, 3],
      },
    };
  }

  private items_: number[];

  override ready() {
    super.ready();
    this.updateScrollableContents();
  }

  private onItemsLengthChanged_() {
    const length = this.$.itemsLengthSlider.value;
    const items: number[] = [];
    for (let i = 0; i < length; i++) {
      items.push(i);
    }
    this.items_ = items;
    this.updateScrollableContents();
  }
}

customElements.define(ScrollViewDemoComponent.is, ScrollViewDemoComponent);
