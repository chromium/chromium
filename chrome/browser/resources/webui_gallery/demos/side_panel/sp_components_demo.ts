// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/icons.html.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_checkbox/cr_checkbox.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_slider/cr_slider.js';
import '//resources/cr_elements/cr_url_list_item/cr_url_list_item.js';
import '//webui-gallery/shared/sp_empty_state.js';
import '//webui-gallery/shared/sp_footer.js';
import '//webui-gallery/shared/sp_heading.js';
import '//webui-gallery/shared/sp_icons.html.js';
import '//webui-gallery/shared/sp_list_item_badge.js';

import type {CrSliderElement} from '//resources/cr_elements/cr_slider/cr_slider.js';
import {CrUrlListItemSize} from '//resources/cr_elements/cr_url_list_item/cr_url_list_item.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './sp_components_demo.css.js';
import {getHtml} from './sp_components_demo.html.js';

interface UrlItem {
  title: string;
  url: string;
}

export interface SpComponentsDemoElement {
  $: {
    itemSizeSelect: HTMLSelectElement,
    urlCountSlider: CrSliderElement,
  };
}

export class SpComponentsDemoElement extends CrLitElement {
  static get is() {
    return 'sp-components-demo';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      hideBackButton_: {type: Boolean},
      itemSize_: {type: String},
      itemSizeOptions_: {type: Array},
      showBadges_: {type: Boolean},
      urlCount_: {type: Number},
      urls_: {type: Array},
    };
  }

  protected hideBackButton_: boolean = true;
  protected itemSize_: CrUrlListItemSize = CrUrlListItemSize.COMPACT;
  protected itemSizeOptions_: CrUrlListItemSize[] = [
    CrUrlListItemSize.COMPACT,
    CrUrlListItemSize.MEDIUM,
    CrUrlListItemSize.LARGE,
  ];
  protected showBadges_: boolean = false;
  protected urlCount_: number = 15;
  protected urls_: UrlItem[] = [];

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedPrivateProperties.has('urlCount_')) {
      this.urls_ = this.computeUrls_();
    }
  }

  private computeUrls_(): UrlItem[] {
    const urls: UrlItem[] = [];
    for (let i = 0; i < this.urlCount_; i++) {
      urls.push({
        title: 'Google',
        url: 'http://www.google.com',
      });
    }
    return urls;
  }

  protected onItemSizeChanged_() {
    this.itemSize_ = this.$.itemSizeSelect.value as CrUrlListItemSize;
  }

  protected onUrlCountChanged_() {
    this.urlCount_ = this.$.urlCountSlider.value;
  }

  protected onHideBackButtonChanged_(e: CustomEvent<{value: boolean}>) {
    this.hideBackButton_ = e.detail.value;
  }

  protected onShowBadgesChanged_(e: CustomEvent<{value: boolean}>) {
    this.showBadges_ = e.detail.value;
  }
}

export const tagName = SpComponentsDemoElement.is;

customElements.define(SpComponentsDemoElement.is, SpComponentsDemoElement);
