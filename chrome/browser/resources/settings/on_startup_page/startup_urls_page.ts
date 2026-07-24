// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-startup-urls-page' is the settings page
 * containing the urls that will be opened when chrome is started.
 */

import 'chrome://resources/js/action_link.js';
import 'chrome://resources/cr_elements/action_link_lit.css.js';
import 'chrome://resources/cr_elements/cr_infinite_list/cr_infinite_list.js';
import '/shared/settings/controls/extension_controlled_indicator.js';
import './startup_url_dialog.js';
import './startup_url_entry.js';

import {PrefServiceObserverMixinLit} from '/shared/settings/prefs2/pref_service_observer_mixin_lit.js';
import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {EDIT_STARTUP_URL_EVENT} from './startup_url_entry.js';
import {getCss} from './startup_urls_page.css.js';
import {getHtml} from './startup_urls_page.html.js';
import type {StartupPageInfo, StartupUrlsPageBrowserProxy} from './startup_urls_page_browser_proxy.js';
import {StartupUrlsPageBrowserProxyImpl} from './startup_urls_page_browser_proxy.js';

export interface SettingsStartupUrlsPageElement {
  $: {
    container: HTMLElement,
  };
}

const SettingsStartupUrlsPageElementBase =
    PrefServiceObserverMixinLit(WebUiListenerMixinLit(CrLitElement));

export class SettingsStartupUrlsPageElement extends
    SettingsStartupUrlsPageElementBase {
  static get is() {
    return 'settings-startup-urls-page';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      startupUrlsPref_: {type: Object},
      startupPages_: {type: Array},
      showStartupUrlDialog_: {type: Boolean},
      startupUrlDialogModel_: {type: Object},
      lastFocused_: {type: Object},
      listBlurred_: {type: Boolean},
      scrollTarget_: {type: Object},
    };
  }

  protected accessor startupUrlsPref_:
      chrome.settingsPrivate.PrefObject<string[]>|undefined;
  protected accessor startupPages_: StartupPageInfo[] = [];
  protected accessor showStartupUrlDialog_: boolean = false;
  protected accessor startupUrlDialogModel_: StartupPageInfo|null = null;
  protected accessor lastFocused_: HTMLElement|null = null;
  protected accessor listBlurred_: boolean = false;
  protected accessor scrollTarget_: HTMLElement = document.documentElement;

  private browserProxy_: StartupUrlsPageBrowserProxy =
      StartupUrlsPageBrowserProxyImpl.getInstance();
  private startupUrlDialogAnchor_: HTMLElement|null = null;

  override connectedCallback() {
    super.connectedCallback();

    this.mirrorPref('session.startup_urls', 'startupUrlsPref_');

    this.addWebUiListener(
        'update-startup-pages', (startupPages: StartupPageInfo[]) => {
          // If an "edit" URL dialog was open, close it, because the underlying
          // page might have just been removed (and model indices have changed
          // anyway).
          if (this.startupUrlDialogModel_) {
            this.onDialogClose_();
          }
          this.startupPages_ = startupPages;
        });
    this.browserProxy_.loadStartupPages();

    this.addEventListener(EDIT_STARTUP_URL_EVENT, (event: Event) => {
      const e =
          event as CustomEvent<{model: StartupPageInfo, anchor: HTMLElement}>;
      this.startupUrlDialogModel_ = e.detail.model;
      this.startupUrlDialogAnchor_ = e.detail.anchor;
      this.showStartupUrlDialog_ = true;
      e.stopPropagation();
    });
  }

  override firstUpdated(changedProperties: PropertyValues<this>) {
    super.firstUpdated(changedProperties);
    this.scrollTarget_ = this.$.container;
  }

  protected onLastFocusedChanged_(e: CustomEvent<{value: HTMLElement | null}>) {
    this.lastFocused_ = e.detail.value;
  }

  protected onListBlurredChanged_(e: CustomEvent<{value: boolean}>) {
    this.listBlurred_ = e.detail.value;
  }

  protected onAddPageClick_(e: Event) {
    e.preventDefault();
    this.showStartupUrlDialog_ = true;
    this.startupUrlDialogAnchor_ =
        this.shadowRoot.querySelector('#addPage a[is=action-link]');
  }

  protected onDialogClose_() {
    this.showStartupUrlDialog_ = false;
    this.startupUrlDialogModel_ = null;
    if (this.startupUrlDialogAnchor_) {
      focusWithoutInk(this.startupUrlDialogAnchor_);
      this.startupUrlDialogAnchor_ = null;
    }
  }

  protected onUseCurrentPagesClick_() {
    this.browserProxy_.useCurrentPages();
  }

  /**
   * @return Whether "Add new page" and "Use current pages" are allowed.
   */
  protected shouldAllowUrlsEdit_(): boolean {
    return this.startupUrlsPref_?.enforcement !==
        chrome.settingsPrivate.Enforcement.ENFORCED;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-startup-urls-page': SettingsStartupUrlsPageElement;
  }
}

customElements.define(
    SettingsStartupUrlsPageElement.is, SettingsStartupUrlsPageElement);
