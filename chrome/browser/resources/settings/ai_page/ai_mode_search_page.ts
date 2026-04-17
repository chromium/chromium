// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import '../controls/settings_toggle_button.js';
import '../settings_columned_section.css.js';
import '../settings_page/settings_section.js';
import '../settings_shared.css.js';
import './ai_site_add_dialog.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import type {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrLazyRenderElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {DomRepeatEvent} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './ai_mode_search_page.html.js';

const SettingsAiModeSearchPageElementBase = PrefsMixin(PolymerElement);

export interface SettingsAiModeSearchPageElement {
  $: {
    menu: CrLazyRenderElement<CrActionMenuElement>,
  };
}

export class SettingsAiModeSearchPageElement extends
    SettingsAiModeSearchPageElementBase {
  static get is() {
    return 'settings-ai-mode-search-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      siteList_: {
        type: Array,
        value: () => [],
      },
      showAddSiteDialog_: {
        type: Boolean,
        value: false,
      },
      siteToEdit_: {
        type: String,
        value: '',
      },
    };
  }

  static get observers() {
    return [
      'onSiteExclusionsChanged_(prefs.contextual_tasks.site_exclusions.value.*)',
    ];
  }

  declare private siteList_: string[];
  declare private showAddSiteDialog_: boolean;
  declare private siteToEdit_: string;

  private onSiteExclusionsChanged_() {
    const exclusions = this.getSiteExclusions();
    this.siteList_ = Object.keys(exclusions).sort();
  }

  private hasSites_(): boolean {
    return this.siteList_.length > 0;
  }

  private onAddSiteClick_() {
    this.siteToEdit_ = '';
    this.showAddSiteDialog_ = true;
  }

  private onMenuClick_(e: DomRepeatEvent<string>) {
    this.siteToEdit_ = e.model.item;
    this.$.menu.get().showAt(e.target as HTMLElement);
  }

  private onEditClick_() {
    this.$.menu.get().close();
    this.showAddSiteDialog_ = true;
  }

  private onRemoveSiteClick_() {
    this.$.menu.get().close();
    this.removeSiteExclusion(this.siteToEdit_);
  }

  private onAddSiteDialogClose_() {
    this.showAddSiteDialog_ = false;
  }

  private onAddSite_(e: CustomEvent<string>) {
    if (this.siteToEdit_ && this.siteToEdit_ !== e.detail) {
      this.removeSiteExclusion(this.siteToEdit_);
    }
    this.addSiteExclusion(e.detail, Date.now());
  }

  getSiteExclusions(): Record<string, number> {
    const pref = this.getPref('contextual_tasks.site_exclusions');
    return pref ? pref.value : {};
  }

  addSiteExclusion(domain: string, timeAddedMs: number) {
    this.setPrefDictEntry(
        'contextual_tasks.site_exclusions', domain, timeAddedMs);
  }

  removeSiteExclusion(domain: string) {
    this.deletePrefDictEntry('contextual_tasks.site_exclusions', domain);
  }

  private onLearnMoreRowClick_() {
    OpenWindowProxyImpl.getInstance().openUrl(
        'https://support.google.com/chrome?p=ai_mode_search');
  }

  private onLearnMoreClick_(event: Event) {
    event.stopPropagation();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-ai-mode-search-page': SettingsAiModeSearchPageElement;
  }
}

customElements.define(
    SettingsAiModeSearchPageElement.is, SettingsAiModeSearchPageElement);
