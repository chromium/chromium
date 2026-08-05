// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-autofill-page-index' is the settings page containing
 * settings for passwords, payment methods, addresses and more.
 */
import 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';
import '/shared/settings/prefs/prefs.js';
import './autofill_page.js';
import '../settings_shared.css.js';

import type {CrViewManagerElement} from 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import {routes} from '../route.js';
import {RouteObserverMixin} from '../router.js';
import type {Route} from '../router.js';
import type {SettingsPlugin} from '../settings_main/settings_plugin.js';
import {SearchableViewContainerMixin} from '../settings_page/searchable_view_container_mixin.js';

import {getTemplate} from './autofill_page_index.html.js';
import {DataManagementSurvey, SavedInfoHandlerImpl} from './saved_info_handler_proxy.js';


export interface SettingsAutofillPageIndexElement {
  $: {
    viewManager: CrViewManagerElement,
  };
}

const SettingsAutofillPageIndexElementBase =
    SearchableViewContainerMixin(RouteObserverMixin(PolymerElement));

export class SettingsAutofillPageIndexElement extends
    SettingsAutofillPageIndexElementBase implements SettingsPlugin {
  static get is() {
    return 'settings-autofill-page-index';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      prefs: Object,

      isShoppingEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('shoppingIntegrationEnabled');
        },
      },

      showSuggestionsFromGeminiSettings_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('showSuggestionsFromGeminiSettings');
        },
      },
    };
  }

  declare prefs: Record<string, unknown>;
  declare private isShoppingEnabled_: boolean;
  declare private showSuggestionsFromGeminiSettings_: boolean;

  override currentRouteChanged(newRoute: Route, oldRoute?: Route) {
    super.currentRouteChanged(newRoute, oldRoute);

    const isFromHomePage = oldRoute?.path === routes.AUTOFILL.path;
    // Need to wait for currentRouteChanged observers on child views to run
    // first, before switching views.
    queueMicrotask(() => {
      switch (newRoute) {
        case routes.AUTOFILL:
          this.$.viewManager.switchView(
              'parent', 'no-animation', 'no-animation');
          SavedInfoHandlerImpl.getInstance().requestDataManagementSurvey(
              DataManagementSurvey.YOUR_SAVED_INFO, isFromHomePage);
          break;
        case routes.BASIC:
          // Switch back to the default views in case they are part of search
          // results.
          this.$.viewManager.switchView(
              'parent', 'no-animation', 'no-animation');
          break;
        case routes.CONTACT_INFO:
          this.$.viewManager.switchView(
              'contactInfo', 'no-animation', 'no-animation');
          SavedInfoHandlerImpl.getInstance().requestDataManagementSurvey(
              DataManagementSurvey.CONTACT_INFO, isFromHomePage);
          break;
        case routes.IDENTITY_DOCS:
          this.$.viewManager.switchView(
              'identityDocs', 'no-animation', 'no-animation');
          SavedInfoHandlerImpl.getInstance().requestDataManagementSurvey(
              DataManagementSurvey.IDENTITY_DOCS, isFromHomePage);
          break;
        // <if expr="is_win or is_macosx">
        case routes.PASSKEYS:
          this.$.viewManager.switchView(
              'passkeys', 'no-animation', 'no-animation');
          break;
        // </if>
        case routes.PAYMENTS:
          this.$.viewManager.switchView(
              'payments', 'no-animation', 'no-animation');
          SavedInfoHandlerImpl.getInstance().requestDataManagementSurvey(
              DataManagementSurvey.PAYMENTS, isFromHomePage);
          break;
        case routes.TRAVEL:
          this.$.viewManager.switchView(
              'travel', 'no-animation', 'no-animation');
          SavedInfoHandlerImpl.getInstance().requestDataManagementSurvey(
              DataManagementSurvey.TRAVEL, isFromHomePage);
          break;
        case routes.SHOPPING:
          assert(this.isShoppingEnabled_);
          this.$.viewManager.switchView(
              'shopping', 'no-animation', 'no-animation');
          break;
        case routes.SUGGESTIONS_FROM_GEMINI:
          assert(this.showSuggestionsFromGeminiSettings_);
          this.$.viewManager.switchView(
              'suggestionsFromGemini', 'no-animation', 'no-animation');
          break;
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
    'settings-autofill-page-index':
        SettingsAutofillPageIndexElement;
  }
}

customElements.define(
    SettingsAutofillPageIndexElement.is,
    SettingsAutofillPageIndexElement);
