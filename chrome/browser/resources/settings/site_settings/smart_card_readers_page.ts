// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../controls/settings_toggle_button.js';
import './smart_card_reader_origin_entry.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {CrDialogElement, SmartCardReaderGrants} from 'lazy_load.js';

import {routes} from '../route.js';
import {RouteObserverMixin} from '../router.js';
import type {Route} from '../router.js';

import {ContentSettingsTypes} from './constants.js';
import {SiteSettingsMixin} from './site_settings_mixin.js';
import {getTemplate} from './smart_card_readers_page.html.js';

export interface SettingsSmartCardReadersPageElement {
  $: {
    confirmReset: CrDialogElement,
    resetButton: CrButtonElement,
  };
}

const SettingsSmartCardReadersPageElementBase =
    SiteSettingsMixin(RouteObserverMixin(WebUiListenerMixin(PolymerElement)));

export class SettingsSmartCardReadersPageElement extends
    SettingsSmartCardReadersPageElementBase {
  static get is() {
    return 'settings-smart-card-readers-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      contentSettingsType_: {
        type: ContentSettingsTypes,
        value: ContentSettingsTypes.SMART_CARD_READERS,
      },
      readersWithGrants_: {
        type: Array,
        value: () => [],
      },
    };
  }

  private readersWithGrants_: SmartCardReaderGrants[];

  override connectedCallback() {
    super.connectedCallback();

    this.addWebUiListener(
        'contentSettingChooserPermissionChanged',
        (category: ContentSettingsTypes) => {
          if (category === ContentSettingsTypes.SMART_CARD_READERS) {
            this.populateList_();
          }
        });
  }

  private onResetSettingsDialogClosed_() {
    const toFocus = this.$.resetButton;
    assert(toFocus);
    focusWithoutInk(toFocus);
  }

  private onCloseDialog_(_e: Event) {
    this.$.confirmReset.close();
  }

  private onClickReset_(e: Event) {
    this.browserProxy.revokeAllSmartCardReadersGrants();
    this.onCloseDialog_(e);
  }

  private onClickShowResetConfirmDialog_(e: Event) {
    e.preventDefault();
    this.$.confirmReset.showModal();
  }

  private async populateList_() {
    await this.browserProxy.getSmartCardReaderGrants().then(
        (grants) => this.set('readersWithGrants_', grants));
  }

  private hasReadersWithGrants_(): boolean {
    return this.readersWithGrants_.length > 0;
  }

  override currentRouteChanged(currentRoute: Route, oldRoute?: Route) {
    if (currentRoute === routes.SITE_SETTINGS_SMART_CARD_READERS &&
        currentRoute !== oldRoute) {
      this.populateList_();
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-smart-card-readers-page': SettingsSmartCardReadersPageElement;
  }
}

customElements.define(
    SettingsSmartCardReadersPageElement.is,
    SettingsSmartCardReadersPageElement);
