// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-ai-page-index' is the settings page containing settings for ai
 * features.
 */
import 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';
import './ai_info_card.js';
import './ai_mode_search_page.js';
import './ai_page.js';
import '../geic_page/geic_page.js';
import '../geic_page/geic_subpage.js';
import '../glic_page/glic_page.js';
import '../glic_page/glic_subpage.js';
import './inline_cue_menu_page.js';

import type {CrViewManagerElement} from 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';
import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {loadTimeData} from '../i18n_setup.js';
import {routes} from '../route.js';
import {RouteObserverMixinLit} from '../router.js';
import type {Route, SettingsRoutes} from '../router.js';
import type {SettingsPlugin} from '../settings_main/settings_plugin.js';
import {SearchableViewContainerMixinLit} from '../settings_page/searchable_view_container_mixin_lit.js';

import {getCss} from './ai_page_index.css.js';
import {getHtml} from './ai_page_index.html.js';


export interface SettingsAiPageIndexElement {
  $: {
    viewManager: CrViewManagerElement,
  };
}

const SettingsAiPageIndexElementBase =
    SearchableViewContainerMixinLit(RouteObserverMixinLit(CrLitElement));

export class SettingsAiPageIndexElement extends SettingsAiPageIndexElementBase
    implements SettingsPlugin {
  static get is() {
    return 'settings-ai-page-index';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      routes_: {type: Object},
      showGlicSettings_: {type: Boolean},
      showGeicSettings_: {type: Boolean},
      showAiPageAiFeatureSection_: {type: Boolean},
      showComposeControl_: {type: Boolean},
      showHistorySearchControl_: {type: Boolean},
      enableAiModeSearchSetting_: {type: Boolean},
      actorLoginFederatedLoginSupportEnabled_: {type: Boolean},
      showAiSuggestionsControl_: {type: Boolean},
      showInlineCueMenuControl_: {type: Boolean},
      showSkillsSettingPage_: {type: Boolean},
      showDictationControl_: {type: Boolean},
    };
  }

  protected accessor routes_: SettingsRoutes = routes;
  protected accessor showGlicSettings_: boolean =
      loadTimeData.getBoolean('showGlicSettings');
  protected accessor showGeicSettings_: boolean =
      loadTimeData.getBoolean('showGeicSettings');
  protected accessor showAiPageAiFeatureSection_: boolean =
      loadTimeData.getBoolean('showAiPageAiFeatureSection');
  protected accessor showComposeControl_: boolean =
      loadTimeData.getBoolean('showComposeControl');
  protected accessor showHistorySearchControl_: boolean =
      loadTimeData.getBoolean('showHistorySearchControl');
  protected accessor enableAiModeSearchSetting_: boolean =
      loadTimeData.getBoolean('enableAiModeSearchSetting');
  protected accessor actorLoginFederatedLoginSupportEnabled_: boolean =
      loadTimeData.getBoolean('actorLoginFederatedLoginSupportEnabled');
  protected accessor showAiSuggestionsControl_: boolean =
      loadTimeData.getBoolean('showAiSuggestionsControl');
  protected accessor showInlineCueMenuControl_: boolean =
      loadTimeData.getBoolean('showInlineCueMenuControl');
  protected accessor showSkillsSettingPage_: boolean =
      loadTimeData.getBoolean('showSkillsSettingPage');
  protected accessor showDictationControl_: boolean =
      loadTimeData.getBoolean('showDictationControl');

  private showDefaultViews_() {
    const defaultViews: string[] = ['aiInfoCard'];

    if (this.showAiPageAiFeatureSection_) {
      defaultViews.push('parent');
    }

    if (this.enableAiModeSearchSetting_) {
      defaultViews.push('aiModeSearch');
    }

    if (this.showGlicSettings_) {
      defaultViews.push('glic');
    }

    if (this.showGeicSettings_) {
      defaultViews.push('geic');
    }

    this.$.viewManager.switchViews(
        defaultViews, 'no-animation', 'no-animation');
  }

  protected shouldShowPermissionsPage_(): boolean {
    return this.showGlicSettings_ &&
        this.actorLoginFederatedLoginSupportEnabled_;
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
        case routes.GEMINI:
          assert(this.showGlicSettings_);
          this.$.viewManager.switchView(
              'gemini', 'no-animation', 'no-animation');
          break;
        case routes.GEMINI_LOGIN:
          assert(this.showGlicSettings_);
          assert(this.actorLoginFederatedLoginSupportEnabled_);
          this.$.viewManager.switchView(
              'geminiLoginPermissions', 'no-animation', 'no-animation');
          break;
        case routes.GEMINI_ENTERPRISE:
          assert(this.showGeicSettings_);
          this.$.viewManager.switchView(
              'geminiEnterprise', 'no-animation', 'no-animation');
          break;
        case routes.AI_SUGGESTIONS:
          assert(this.showAiSuggestionsControl_);
          this.$.viewManager.switchView(
              'aiSuggestions', 'no-animation', 'no-animation');
          break;
        case routes.INLINE_CUE_MENU:
          assert(this.showInlineCueMenuControl_);
          this.$.viewManager.switchView(
              'inlineCueMenu', 'no-animation', 'no-animation');
          break;
        case routes.SKILLS:
          assert(this.showSkillsSettingPage_);
          this.$.viewManager.switchView(
              'skills', 'no-animation', 'no-animation');
          break;
        case routes.DICTATION:
          assert(this.showDictationControl_);
          this.$.viewManager.switchView(
              'dictation', 'no-animation', 'no-animation');
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
    'settings-ai-page-index': SettingsAiPageIndexElement;
  }
}

customElements.define(
    SettingsAiPageIndexElement.is, SettingsAiPageIndexElement);
