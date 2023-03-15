// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/icons.html.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_checkbox/cr_checkbox.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_slider/cr_slider.js';
import '//resources/cr_elements/cr_url_list_item/cr_url_list_item.js';
import '//resources/cr_elements/md_select.css.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '//webui-gallery/shared/sp_filter_chip.js';
import '//webui-gallery/shared/sp_footer.js';
import '//webui-gallery/shared/sp_heading.js';
import '//webui-gallery/shared/sp_icons.html.js';
import '//webui-gallery/shared/sp_list_item_badge.js';
import '//webui-gallery/shared/sp_shared_style.css.js';
import '//webui-gallery/shared/sp_shared_vars.css.js';

import {CrSliderElement} from '//resources/cr_elements/cr_slider/cr_slider.js';
import {CrUrlListItemSize} from '//resources/cr_elements/cr_url_list_item/cr_url_list_item.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './sp_components_demo.html.js';

interface UrlItem {
  title: string;
  url: string;
}

interface SpComponentsDemo {
  $: {
    itemSizeSelect: HTMLSelectElement,
    urlCountSlider: CrSliderElement,
  };
}

class SpComponentsDemo extends PolymerElement {
  static get is() {
    return 'sp-components-demo';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      hideBackButton_: {
        type: Boolean,
        value: true,
      },
      itemSize_: {
        type: String,
        value: CrUrlListItemSize.COMPACT,
      },
      itemSizeOptions_: {
        type: Array,
        value: [
          CrUrlListItemSize.COMPACT,
          CrUrlListItemSize.MEDIUM,
          CrUrlListItemSize.LARGE,
        ],
      },
      showBadges_: Boolean,
      urlCount_: {
        type: Number,
        value: 15,
      },
      urls_: {
        type: Array,
        computed: 'computeUrls_(urlCount_)',
      },
    };
  }

  private hideBackButton_: boolean;
  private itemSize_: CrUrlListItemSize;
  private itemSizeOptions_: CrUrlListItemSize[];
  private showBadges_: boolean;
  private urlCount_: number;
  private urls_: UrlItem[];

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

  private onItemSizeChanged_() {
    this.itemSize_ = this.$.itemSizeSelect.value as CrUrlListItemSize;
  }

  private onUrlCountChanged_() {
    this.urlCount_ = this.$.urlCountSlider.value;
  }
}

customElements.define(SpComponentsDemo.is, SpComponentsDemo);
