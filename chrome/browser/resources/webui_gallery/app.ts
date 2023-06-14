// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_menu_selector/cr_menu_selector.js';
import '//resources/cr_elements/cr_toggle/cr_toggle.js';
import '//resources/cr_elements/cr_nav_menu_item_style.css.js';
import '//resources/cr_elements/cr_shared_style.css.js';
import '//resources/polymer/v3_0/iron-location/iron-location.js';

import {ColorChangeUpdater, COLORS_CSS_SELECTOR} from '//resources/cr_components/color_change_listener/colors_css_updater.js';
import {CrMenuSelector} from '//resources/cr_elements/cr_menu_selector/cr_menu_selector.js';
import {assert} from '//resources/js/assert_ts.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';

interface Demo {
  name: string;
  path: string;
  src: string;
}

export interface WebuiGalleryAppElement {
  $: {
    main: HTMLElement,
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
      demos_: {
        type: Array,
        value: function() {
          return [
            {
              name: 'Accessibility Live Regions',
              path: 'a11y',
              src: 'cr_a11y_announcer/cr_a11y_announcer_demo.js',
            },
            {
              name: 'Action Menus',
              path: 'action-menus',
              src: 'cr_action_menu/cr_action_menu_demo.js',
            },
            {
              name: 'Buttons',
              path: 'buttons',
              src: 'buttons/buttons_demo.js',
            },
            {
              name: 'Cards and rows',
              path: 'cards',
              src: 'card/card_demo.js',
            },
            {
              name: 'Checkboxes',
              path: 'checkboxes',
              src: 'cr_checkbox/cr_checkbox_demo.js',
            },
            {
              name: 'Dialogs',
              path: 'dialogs',
              src: 'cr_dialog/cr_dialog_demo.js',
            },
            {
              name: 'Icons',
              path: 'icons',
              src: 'cr_icons/cr_icons_demo.js',
            },
            {
              name: 'Inputs',
              path: 'inputs',
              src: 'cr_input/cr_input_demo.js',
            },
            {
              name: 'Navigation menus',
              path: 'nav-menus',
              src: 'nav_menu/nav_menu_demo.js',
            },
            {
              name: 'Progress indicators, Polymer',
              path: 'progress-polymer',
              src: 'progress_indicators/progress_indicator_polymer_demo.js',
            },
            {
              name: 'Progress indicators, non-Polymer',
              path: 'progress-nonpolymer',
              src: 'progress_indicators/progress_indicator_native_demo.js',
            },
            {
              name: 'Radio buttons and groups',
              path: 'radios',
              src: 'cr_radio/cr_radio_demo.js',
            },
            {
              name: 'Scroll view',
              path: 'scroll-view',
              src: 'scroll_view/scroll_view_demo.js',
            },
            {
              name: 'Select menu',
              path: 'select-menu',
              src: 'md_select/md_select_demo.js',
            },
            {
              name: 'Side panel',
              path: 'side-panel',
              src: 'side_panel/sp_components_demo.js',
            },
            {
              name: 'Sliders',
              path: 'sliders',
              src: 'cr_slider/cr_slider_demo.js',
            },
            {
              name: 'Tabs, non-Polymer',
              path: 'tabs1',
              src: 'cr_tab_box/cr_tab_box_demo.js',
            },
            {
              name: 'Tabs, Polymer',
              path: 'tabs2',
              src: 'cr_tabs/cr_tabs_demo.js',
            },
            {
              name: 'Toast',
              path: 'toast',
              src: 'cr_toast/cr_toast_demo.js',
            },
            {
              name: 'Toggles',
              path: 'toggles',
              src: 'cr_toggle/cr_toggle_demo.js',
            },
            {
              name: 'Toolbar',
              path: 'toolbar',
              src: 'cr_toolbar/cr_toolbar_demo.js',
            },
            {
              name: 'Tree, non-Polymer',
              path: 'tree',
              src: 'cr_tree/cr_tree_demo.js',
            },
            {
              name: 'URL List Item',
              path: 'url-list-item',
              src: 'cr_url_list_item/cr_url_list_item_demo.js',
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

  private demos_: Demo[];
  private path_: string;

  override ready() {
    super.ready();
    ColorChangeUpdater.forDocument().start();
  }

  private async onPathChanged_() {
    const path = this.path_.substring(1);
    const demoIndex =
        path === '' ? 0 : this.demos_.findIndex(demo => demo.path === path);
    assert(demoIndex !== -1);
    this.$.selector.selected = demoIndex;

    const demo = this.demos_[demoIndex]!;

    const {tagName} = await import(`./demos/${demo.src}`);
    assert(tagName);

    const demoElement = document.createElement(tagName);
    this.$.main.innerHTML = window.trustedTypes!.emptyHTML;
    this.$.main.appendChild(demoElement);
  }

  private onFollowColorPipelineChange_(e: CustomEvent<boolean>) {
    if (e.detail) {
      assert(document.body.querySelector(COLORS_CSS_SELECTOR) === null);
      const link = document.createElement('link');
      link.rel = 'stylesheet';
      link.href = 'chrome://theme/colors.css?sets=ui,chrome';
      document.body.appendChild(link);
      return;
    }

    const link = document.body.querySelector(COLORS_CSS_SELECTOR);
    assert(link);
    link.remove();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'webui-gallery-app': WebuiGalleryAppElement;
  }
}

customElements.define(WebuiGalleryAppElement.is, WebuiGalleryAppElement);
