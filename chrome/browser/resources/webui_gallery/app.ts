// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_menu_selector/cr_menu_selector.js';
import '//resources/cr_elements/cr_nav_menu_item_style.css.js';
import '//resources/polymer/v3_0/iron-location/iron-location.js';
import '//resources/polymer/v3_0/iron-selector/iron-selector.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {IronSelectorElement} from 'chrome://resources/polymer/v3_0/iron-selector/iron-selector.js';

import {getTemplate} from './app.html.js';

export interface WebuiGalleryAppElement {
  $: {
    iframe: HTMLIFrameElement,
    selector: IronSelectorElement,
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
              src: 'cr_a11y_announcer_demo.html',
            },
            {
              name: 'Action Menus',
              path: 'action-menus',
              src: 'cr_action_menu_demo.html',
            },
            {
              name: 'Buttons',
              path: 'buttons',
              src: 'cr_button_demo.html',
            },
            {
              name: 'Checkboxes',
              path: 'checkboxes',
              src: 'cr_checkbox_demo.html',
            },
            {
              name: 'Dialogs',
              path: 'dialogs',
              src: 'cr_dialog_demo.html',
            },
            {
              name: 'Inputs',
              path: 'inputs',
              src: 'cr_input/cr_input_demo.html',
            },
            {
              name: 'Radio buttons and groups',
              path: 'radios',
              src: 'cr_radio_demo.html',
            },
            {
              name: 'Sliders',
              path: 'sliders',
              src: 'cr_slider/cr_slider_demo.html',
            },
            {
              name: 'Tabs, non Polymer',
              path: 'tabs1',
              src: 'cr_tab_box/cr_tab_box_demo.html',
            },
            {
              name: 'Toggles',
              path: 'toggles',
              src: 'cr_toggle_demo.html',
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
