// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import '../controls/settings_toggle_button.js';
import '../settings_page/settings_section.js';
import './ai_site_add_dialog.js';
import '/shared/settings/controls/cr_policy_pref_indicator.js';

import {PrefService} from '/shared/settings/prefs2/pref_service.js';
import {PrefServiceObserverMixinLit} from '/shared/settings/prefs2/pref_service_observer_mixin_lit.js';
import type {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrLazyRenderLitElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './ai_mode_search_page.css.js';
import {getHtml} from './ai_mode_search_page.html.js';

const SettingsAiModeSearchPageElementBase =
    PrefServiceObserverMixinLit(CrLitElement);

export interface SettingsAiModeSearchPageElement {
  $: {
    menu: CrLazyRenderLitElement<CrActionMenuElement>,
  };
}

export class SettingsAiModeSearchPageElement extends
    SettingsAiModeSearchPageElementBase {
  static get is() {
    return 'settings-ai-mode-search-page';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      siteList_: {type: Array},
      showAddSiteDialog_: {type: Boolean},
      siteToEdit_: {type: String},
      enterprisePref_: {type: Object},
    };
  }

  protected accessor siteList_: string[] = [];
  protected accessor showAddSiteDialog_: boolean = false;
  protected accessor siteToEdit_: string = '';
  protected accessor enterprisePref_: chrome.settingsPrivate.PrefObject|
      undefined;

  override connectedCallback() {
    super.connectedCallback();
    this.mirrorPref(
        'contextual_tasks.smart_tab_sharing_settings', 'enterprisePref_');
    this.addPrefObserver(
        'contextual_tasks.site_exclusions',
        () => this.onSiteExclusionsChanged_());
  }

  protected isDisabledByPolicy_(): boolean {
    return !!this.enterprisePref_ && this.enterprisePref_.value === 1;
  }

  private onSiteExclusionsChanged_() {
    const exclusions = this.getSiteExclusions();
    this.siteList_ = Object.keys(exclusions).sort();
  }

  protected hasSites_(): boolean {
    return this.siteList_.length > 0;
  }

  protected onAddSiteClick_() {
    this.siteToEdit_ = '';
    this.showAddSiteDialog_ = true;
  }

  protected onMenuClick_(e: Event) {
    const target = e.currentTarget as HTMLElement;
    this.siteToEdit_ = target.dataset['site']!;
    this.$.menu.get().showAt(target);
  }

  protected onEditClick_() {
    this.$.menu.get().close();
    this.showAddSiteDialog_ = true;
  }

  protected onRemoveSiteClick_() {
    this.$.menu.get().close();
    this.removeSiteExclusion(this.siteToEdit_);
  }

  protected onAddSiteDialogClose_() {
    this.showAddSiteDialog_ = false;
  }

  protected onAddSite_(e: CustomEvent<string>) {
    if (this.siteToEdit_ && this.siteToEdit_ !== e.detail) {
      this.removeSiteExclusion(this.siteToEdit_);
    }
    this.addSiteExclusion(e.detail, Date.now());
  }

  getSiteExclusions(): Record<string, number> {
    const pref = PrefService.getInstance().getPref<Record<string, number>>(
        'contextual_tasks.site_exclusions');
    return pref ? pref.value : {};
  }

  addSiteExclusion(domain: string, timeAddedMs: number) {
    PrefService.getInstance().setPrefDictEntry(
        'contextual_tasks.site_exclusions', domain, timeAddedMs);
  }

  removeSiteExclusion(domain: string) {
    PrefService.getInstance().deletePrefDictEntry(
        'contextual_tasks.site_exclusions', domain);
  }

  protected onLearnMoreRowClick_() {
    OpenWindowProxyImpl.getInstance().openUrl(
        'https://support.google.com/chrome?p=ai_mode_search');
  }

  protected onLearnMoreClick_(event: Event) {
    event.stopPropagation();
  }
}

export type AiModeSearchPageElement = SettingsAiModeSearchPageElement;

declare global {
  interface HTMLElementTagNameMap {
    'settings-ai-mode-search-page': SettingsAiModeSearchPageElement;
  }
}

customElements.define(
    SettingsAiModeSearchPageElement.is, SettingsAiModeSearchPageElement);
