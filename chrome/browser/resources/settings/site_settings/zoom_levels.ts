// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'zoom-levels' is the polymer element for showing the sites that are zoomed in
 * or out.
 */

import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/icons_lit.html.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import '../settings_shared.css.js';
import '../site_favicon.js';

import {ListPropertyUpdateMixin} from 'chrome://resources/cr_elements/list_property_update_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import type {IronListElement} from 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import type {DomRepeatEvent} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SiteSettingsMixin} from './site_settings_mixin.js';
import type {ZoomLevelEntry} from './site_settings_prefs_browser_proxy.js';
import {getTemplate} from './zoom_levels.html.js';

export interface ZoomLevelsElement {
  $: {
    empty: HTMLElement,
    listContainer: HTMLElement,
    list: IronListElement,
  };
}

const ZoomLevelsElementBase = ListPropertyUpdateMixin(
    SiteSettingsMixin(WebUiListenerMixin(PolymerElement)));

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

  private sites_: ZoomLevelEntry[];
  private showNoSites_: boolean;

  override ready() {
    super.ready();

    this.addWebUiListener(
        'onZoomLevelsChanged',
        (sites: ZoomLevelEntry[]) => this.onZoomLevelsChanged_(sites));
    this.browserProxy.fetchZoomLevels();
  }

  /**
   * A handler for when zoom levels change.
   * @param sites The up to date list of sites and their zoom levels.
   */
  private onZoomLevelsChanged_(sites: ZoomLevelEntry[]) {
    this.updateList('sites_', item => item.hostOrSpec, sites);
    this.showNoSites_ = this.sites_.length === 0;
  }

  /**
   * A handler for when a zoom level for a site is deleted.
   */
  private removeZoomLevel_(event: DomRepeatEvent<ZoomLevelEntry>) {
    const site = this.sites_[event.model.index];
    this.browserProxy.removeZoomLevel(site.hostOrSpec);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'zoom-levels': ZoomLevelsElement;
  }
}

customElements.define(ZoomLevelsElement.is, ZoomLevelsElement);
