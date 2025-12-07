// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-ai-page-index' is the settings page containing settings for
 * passwords, payment methods and addresses.
 */
import 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';
import '/shared/settings/prefs/prefs.js';
import './ai_info_card.js';
import './ai_page.js';
// <if expr="enable_glic">
import '../glic_page/glic_page.js';
import '../glic_page/glic_subpage.js';

// </if>

import type {CrViewManagerElement} from 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import {routes} from '../route.js';
import {RouteObserverMixin} from '../router.js';
import type {Route, SettingsRoutes} from '../router.js';
import type {SettingsPlugin} from '../settings_main/settings_plugin.js';
import {SearchableViewContainerMixin} from '../settings_page/searchable_view_container_mixin.js';

import {getTemplate} from './ai_page_index.html.js';


export interface SettingsAiPageIndexElement {
  $: {
    viewManager: CrViewManagerElement,
  };
}

const SettingsAiPageIndexElementBase =
    SearchableViewContainerMixin(RouteObserverMixin(PolymerElement));

export class SettingsAiPageIndexElement extends SettingsAiPageIndexElementBase
    implements SettingsPlugin {
  static get is() {
    return 'settings-ai-page-index';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      prefs: Object,

      routes_: {
        type: Object,
        value: () => routes,
      },

      // <if expr="enable_glic">
      showGlicSettings_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('showGlicSettings'),
      },
      // </if>

      showAiPageAiFeatureSection_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('showAiPageAiFeatureSection'),
      },

      showComposeControl_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('showComposeControl'),
      },

      showCompareControl_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('showCompareControl'),
      },

      showHistorySearchControl_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('showHistorySearchControl'),
      },

      showTabOrganizationControl_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('showTabOrganizationControl'),
      },
    };
  }

  declare prefs: {[key: string]: any};
  declare private routes_: SettingsRoutes;
  // <if expr="enable_glic">
  declare private showGlicSettings_: boolean;
  // </if>
  declare private showAiPageAiFeatureSection_: boolean;
  declare private showComposeControl_: boolean;
  declare private showCompareControl_: boolean;
  declare private showHistorySearchControl_: boolean;
  declare private showTabOrganizationControl_: boolean;

  private showDefaultViews_() {
    const defaultViews: string[] = ['aiInfoCard'];

    if (this.showAiPageAiFeatureSection_) {
      defaultViews.push('parent');
    }

    // <if expr="enable_glic">
    if (this.showGlicSettings_) {
      defaultViews.push('glic');
    }
    // </if>

    this.$.viewManager.switchViews(
        defaultViews, 'no-animation', 'no-animation');
  }

  override currentRouteChanged(newRoute: Route, oldRoute?: Route) {
    super.currentRouteChanged(newRoute, oldRoute);

    // Need to wait for currentRouteChanged observers on child views to run
    // first, before switching views.
    queueMicrotask(() => {
      switch (newRoute) {
        case routes.AI:
          this.showDefaultViews_();
          break;
        case routes.BASIC:
          // Switch back to the default view in case they are part of search
          // results.
          this.showDefaultViews_();
          break;
        case routes.AI_TAB_ORGANIZATION:
          assert(this.showTabOrganizationControl_);
          this.$.viewManager.switchView(
              'tabOrganization', 'no-animation', 'no-animation');
          break;
        case routes.HISTORY_SEARCH:
          assert(this.showHistorySearchControl_);
          this.$.viewManager.switchView(
              'historySearch', 'no-animation', 'no-animation');
          break;
        case routes.OFFER_WRITING_HELP:
          assert(this.showComposeControl_);
          this.$.viewManager.switchView(
              'compose', 'no-animation', 'no-animation');
          break;
        case routes.COMPARE:
          assert(this.showCompareControl_);
          this.$.viewManager.switchView(
              'compare', 'no-animation', 'no-animation');
          break;
        // <if expr="enable_glic">
        case routes.GEMINI:
          assert(this.showGlicSettings_);
          this.$.viewManager.switchView(
              'gemini', 'no-animation', 'no-animation');
          break;
        // </if>
        default:
          // Nothing to do. Other parent elements are responsible for updating
          // the displayed contents.
          break;
      }
    });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-ai-page-index': SettingsAiPageIndexElement;
  }
}

customElements.define(
    SettingsAiPageIndexElement.is, SettingsAiPageIndexElement);
