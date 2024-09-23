// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-startup-urls-page' is the settings page
 * containing the urls that will be opened when chrome is started.
 */

import 'chrome://resources/js/action_link.js';
import 'chrome://resources/cr_elements/action_link.css.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import '/shared/settings/controls/extension_controlled_indicator.js';
import '../settings_shared.css.js';
import './startup_url_dialog.js';

import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ScrollableMixin} from '../scrollable_mixin.js';

import {EDIT_STARTUP_URL_EVENT} from './startup_url_entry.js';
import {getTemplate} from './startup_urls_page.html.js';
import type {StartupPageInfo, StartupUrlsPageBrowserProxy} from './startup_urls_page_browser_proxy.js';
import {StartupUrlsPageBrowserProxyImpl} from './startup_urls_page_browser_proxy.js';


const SettingsStartupUrlsPageElementBase =
    ScrollableMixin(WebUiListenerMixin(PolymerElement));

export class SettingsStartupUrlsPageElement extends
    SettingsStartupUrlsPageElementBase {
  static get is() {
    return 'settings-startup-urls-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      prefs: Object,

      /**
       * Pages to load upon browser startup.
       */
      startupPages_: Array,

      showStartupUrlDialog_: Boolean,
      startupUrlDialogModel_: Object,
      lastFocused_: Object,
      listBlurred_: Boolean,
    };
  }

  prefs: Object;
  private startupPages_: StartupPageInfo[];
  private showStartupUrlDialog_: boolean;
  private startupUrlDialogModel_: StartupPageInfo|null;
  private lastFocused_: HTMLElement;
  private listBlurred_: boolean;
  private browserProxy_: StartupUrlsPageBrowserProxy =
      StartupUrlsPageBrowserProxyImpl.getInstance();
  private startupUrlDialogAnchor_: HTMLElement|null;

  constructor() {
    super();

    /**
     * The element to return focus to, when the startup-url-dialog is closed.
     */
    this.startupUrlDialogAnchor_ = null;
  }

  override connectedCallback() {
    super.connectedCallback();

    this.addWebUiListener(
        'update-startup-pages', (startupPages: StartupPageInfo[]) => {
          // If an "edit" URL dialog was open, close it, because the underlying
          // page might have just been removed (and model indices have changed
          // anyway).
          if (this.startupUrlDialogModel_) {
            this.destroyUrlDialog_();
          }
          this.startupPages_ = startupPages;
          this.updateScrollableContents();
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

  private onAddPageClick_(e: Event) {
    e.preventDefault();
    this.showStartupUrlDialog_ = true;
    this.startupUrlDialogAnchor_ =
        this.shadowRoot!.querySelector('#addPage a[is=action-link]');
  }

  private destroyUrlDialog_() {
    this.showStartupUrlDialog_ = false;
    this.startupUrlDialogModel_ = null;
    if (this.startupUrlDialogAnchor_) {
      focusWithoutInk(this.startupUrlDialogAnchor_);
      this.startupUrlDialogAnchor_ = null;
    }
  }

  private onUseCurrentPagesClick_() {
    this.browserProxy_.useCurrentPages();
  }

  /**
   * @return Whether "Add new page" and "Use current pages" are allowed.
   */
  private shouldAllowUrlsEdit_(): boolean {
    return this.get('prefs.session.startup_urls.enforcement') !==
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
