// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'zoom-levels' is the polymer element for showing the sites that are zoomed in
 * or out.
 */

import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import '../settings_shared_css.js';
import '../site_favicon.js';

import {ListPropertyUpdateMixin} from 'chrome://resources/js/list_property_update_mixin.js';
import {WebUIListenerMixin} from 'chrome://resources/js/web_ui_listener_mixin.js';
import {IronListElement} from 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import {DomRepeatEvent, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SiteSettingsMixin} from './site_settings_mixin.js';
import {ZoomLevelEntry} from './site_settings_prefs_browser_proxy.js';
import {getTemplate} from './zoom_levels.html.js';

export interface ZoomLevelsElement {
  $: {
    empty: HTMLElement,
    listContainer: HTMLElement,
    list: IronListElement,
  };
}

const ZoomLevelsElementBase = ListPropertyUpdateMixin(
    SiteSettingsMixin(WebUIListenerMixin(PolymerElement)));

export class ZoomLevelsElement extends ZoomLevelsElementBase {
  static get is() {
    return 'zoom-levels';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Array of sites that are zoomed in or out.
       */
      sites_: {
        type: Array,
        value: () => [],
      },

      showNoSites_: {
        type: Boolean,
        value: false,
      },
    };
  }

  private sites_: Array<ZoomLevelEntry>;
  private showNoSites_: boolean;

  override ready() {
    super.ready();

    this.addWebUIListener(
        'onZoomLevelsChanged',
        (sites: Array<ZoomLevelEntry>) => this.onZoomLevelsChanged_(sites));
    this.browserProxy.fetchZoomLevels();
  }

  /**
   * A handler for when zoom levels change.
   * @param sites The up to date list of sites and their zoom levels.
   */
  private onZoomLevelsChanged_(sites: Array<ZoomLevelEntry>) {
    this.updateList('sites_', item => item.origin, sites);
    this.showNoSites_ = this.sites_.length === 0;
  }

  /**
   * A handler for when a zoom level for a site is deleted.
   */
  private removeZoomLevel_(event: DomRepeatEvent<ZoomLevelEntry>) {
    const site = this.sites_[event.model.index];
    this.browserProxy.removeZoomLevel(site.origin);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'zoom-levels': ZoomLevelsElement;
  }
}

customElements.define(ZoomLevelsElement.is, ZoomLevelsElement);
