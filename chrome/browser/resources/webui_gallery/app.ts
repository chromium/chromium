// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_menu_selector/cr_menu_selector.js';
import '//resources/cr_elements/cr_nav_menu_item_style.css.js';
import '//resources/polymer/v3_0/iron-selector/iron-selector.js';

import {assert} from '//resources/js/assert_ts.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';

export class WebuiGalleryAppElement extends PolymerElement {
  static get is() {
    return 'webui-gallery-app';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      demos: {
        type: Array,
        value: function() {
          return [
            {
              name: 'cr-button demo',
              url: 'cr_button_demo.html',
            },
            {
              name: 'cr-checkbox demo',
              url: 'cr_checkbox_demo.html',
            },
            {
              name: 'cr-dialog demo',
              url: 'cr_dialog_demo.html',
            }
          ];
        },
      },

      currentSrc_: String,
    };
  }

  private currentSrc_: string;

  private onDomChange_() {
    const first = this.shadowRoot!.querySelector('a');
    assert(first);
    const selector = this.shadowRoot!.querySelector('iron-selector');
    assert(selector);
    selector.selected = first.href;
    this.currentSrc_ = first.href;
  }

  private onLinkClick_(e: Event) {
    e.preventDefault();
  }

  private onSelectorActivate_(e: CustomEvent<{selected: string}>) {
    const demoUrl = e.detail.selected;
    this.currentSrc_ = demoUrl;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'webui-gallery-app': WebuiGalleryAppElement;
  }
}

customElements.define(WebuiGalleryAppElement.is, WebuiGalleryAppElement);
