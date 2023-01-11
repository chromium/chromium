// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_menu_selector/cr_menu_selector.js';
import '//resources/cr_elements/cr_nav_menu_item_style.css.js';
import '//resources/polymer/v3_0/iron-location/iron-location.js';

import {CrMenuSelector} from '//resources/cr_elements/cr_menu_selector/cr_menu_selector.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';

export interface WebuiGalleryAppElement {
  $: {
    iframe: HTMLIFrameElement,
    selector: CrMenuSelector,
  };
}

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
              name: 'Accessibility Live Regions',
              path: 'a11y',
              src: 'cr_a11y_announcer/index.html',
            },
            {
              name: 'Action Menus',
              path: 'action-menus',
              src: 'cr_action_menu/index.html',
            },
            {
              name: 'Buttons',
              path: 'buttons',
              src: 'buttons_demo.html',
            },
            {
              name: 'Cards and rows',
              path: 'cards',
              src: 'card/index.html',
            },
            {
              name: 'Checkboxes',
              path: 'checkboxes',
              src: 'cr_checkbox_demo.html',
            },
            {
              name: 'Dialogs',
              path: 'dialogs',
              src: 'cr_dialog/index.html',
            },
            {
              name: 'Icons',
              path: 'icons',
              src: 'cr_icons/index.html',
            },
            {
              name: 'Inputs',
              path: 'inputs',
              src: 'cr_input/index.html',
            },
            {
              name: 'Navigation menus',
              path: 'nav-menus',
              src: 'nav_menu/index.html',
            },
            {
              name: 'Progress indicators, Polymer',
              path: 'progress-polymer',
              src: 'progress_indicator_polymer_demo.html',
            },
            {
              name: 'Progress indicators, non-Polymer',
              path: 'progress-nonpolymer',
              src: 'progress_indicator_nonpolymer_demo.html',
            },
            {
              name: 'Radio buttons and groups',
              path: 'radios',
              src: 'cr_radio_demo.html',
            },
            {
              name: 'Scroll view',
              path: 'scroll-view',
              src: 'scroll_view/index.html',
            },
            {
              name: 'Select menu',
              path: 'select-menu',
              src: 'md_select/index.html',
            },
            {
              name: 'Sliders',
              path: 'sliders',
              src: 'cr_slider/index.html',
            },
            {
              name: 'Tabs, non-Polymer',
              path: 'tabs1',
              src: 'cr_tab_box/index.html',
            },
            {
              name: 'Tabs, Polymer',
              path: 'tabs2',
              src: 'cr_tabs/index.html',
            },
            {
              name: 'Toast',
              path: 'toast',
              src: 'cr_toast/index.html',
            },
            {
              name: 'Toggles',
              path: 'toggles',
              src: 'cr_toggle_demo.html',
            },
            {
              name: 'Toolbar',
              path: 'toolbar',
              src: 'cr_toolbar/index.html',
            },
            {
              name: 'Tree, non-Polymer',
              path: 'tree',
              src: 'cr_tree/index.html',
            },
            {
              name: 'URL List Item',
              path: 'url-list-item',
              src: 'cr_url_list_item/index.html',
            },
          ];
        },
      },

      path_: {
        type: String,
        observer: 'onPathChanged_',
      },
    };
  }

  demos: Array<{name: string, path: string, src: string}>;
  private path_: string;

  private onPathChanged_() {
    const selectedIndex = Math.max(
        0, this.demos.findIndex(demo => demo.path === this.path_.substring(1)));
    this.$.selector.selected = selectedIndex;

    // Use `location.replace` instead of iframe's `src` to prevent the iframe's
    // source from interfering with tab's history.
    this.$.iframe.contentWindow!.location.replace(
        'demos/' + this.demos[selectedIndex]!.src);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'webui-gallery-app': WebuiGalleryAppElement;
  }
}

customElements.define(WebuiGalleryAppElement.is, WebuiGalleryAppElement);
