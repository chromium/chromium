// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides behavior to fetch the list of user specified permitted
 * and restricted sites on creation and when these lists are updated. Used by
 * multiple pages.
 */

import type {CrLitElement, PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import type {ChromeEvent} from '/tools/typescript/definitions/chrome_event.js';

import type {ItemDelegate} from '../item.js';
import {DummyItemDelegate, FakeChromeEvent} from '../item.js';

export interface SiteSettingsDelegate {
  getUserSiteSettings(): Promise<chrome.developerPrivate.UserSiteSettings>;
  addUserSpecifiedSites(
      siteSet: chrome.developerPrivate.SiteSet, hosts: string[]): Promise<void>;
  removeUserSpecifiedSites(
      siteSet: chrome.developerPrivate.SiteSet, hosts: string[]): Promise<void>;
  getUserAndExtensionSitesByEtld():
      Promise<chrome.developerPrivate.SiteGroup[]>;
  getMatchingExtensionsForSite(site: string):
      Promise<chrome.developerPrivate.MatchingExtensionInfo[]>;
  updateSiteAccess(
      site: string,
      updates: chrome.developerPrivate.ExtensionSiteAccessUpdate[]):
      Promise<void>;
  getUserSiteSettingsChangedTarget():
      ChromeEvent<(settings: chrome.developerPrivate.UserSiteSettings) => void>;
}

export class DummySiteSettingsDelegate {
  getUserSiteSettings() {
    return Promise.resolve({permittedSites: [], restrictedSites: []});
  }
  addUserSpecifiedSites(
      _siteSet: chrome.developerPrivate.SiteSet, _hosts: string[]) {
    return Promise.resolve();
  }
  removeUserSpecifiedSites(
      _siteSet: chrome.developerPrivate.SiteSet, _hosts: string[]) {
    return Promise.resolve();
  }
  getUserAndExtensionSitesByEtld() {
    return Promise.resolve([]);
  }
  getMatchingExtensionsForSite(_site: string) {
    return Promise.resolve([]);
  }
  updateSiteAccess(
      _site: string,
      _updates: chrome.developerPrivate.ExtensionSiteAccessUpdate[]) {
    return Promise.resolve();
  }
  getUserSiteSettingsChangedTarget() {
    return new FakeChromeEvent();
  }
}

// Have to reproduce DummySiteSettingsDelegate since TS does not allow
// extending multiple classes.
export class DummySiteSettingsMixinDelegate extends DummyItemDelegate {
  getUserSiteSettings() {
    return Promise.resolve({permittedSites: [], restrictedSites: []});
  }
  addUserSpecifiedSites(
      _siteSet: chrome.developerPrivate.SiteSet, _hosts: string[]) {
    return Promise.resolve();
  }
  removeUserSpecifiedSites(
      _siteSet: chrome.developerPrivate.SiteSet, _hosts: string[]) {
    return Promise.resolve();
  }
  getUserAndExtensionSitesByEtld() {
    return Promise.resolve([]);
  }
  getMatchingExtensionsForSite(_site: string) {
    return Promise.resolve([]);
  }
  updateSiteAccess(
      _site: string,
      _updates: chrome.developerPrivate.ExtensionSiteAccessUpdate[]) {
    return Promise.resolve();
  }
  getUserSiteSettingsChangedTarget() {
    return new FakeChromeEvent();
  }
}

type Constructor<T> = new (...args: any[]) => T;

export const SiteSettingsMixin =
    <T extends Constructor<CrLitElement>>(superClass: T): T&
    Constructor<SiteSettingsMixinInterface> => {
      class SiteSettingsMixin extends superClass implements
          SiteSettingsMixinInterface {
        static get properties() {
          return {
            delegate: {type: Object},
            enableEnhancedSiteControls: {type: Boolean},
            restrictedSites: {type: Array},
            permittedSites: {type: Array},
          };
        }

        delegate: ItemDelegate&SiteSettingsDelegate =
            new DummySiteSettingsMixinDelegate();
        enableEnhancedSiteControls: boolean = false;
        restrictedSites: string[] = [];
        permittedSites: string[] = [];

        override firstUpdated(changedProperties: PropertyValues<this>) {
          super.firstUpdated(changedProperties);

          if (this.enableEnhancedSiteControls) {
            this.delegate.getUserSiteSettings().then(
                this.onUserSiteSettingsChanged_.bind(this));
            this.delegate.getUserSiteSettingsChangedTarget().addListener(
                this.onUserSiteSettingsChanged_.bind(this));
          }
        }

        private onUserSiteSettingsChanged_({
          permittedSites,
          restrictedSites,
        }: chrome.developerPrivate.UserSiteSettings) {
          this.permittedSites = permittedSites;
          this.restrictedSites = restrictedSites;
        }
      }

      return SiteSettingsMixin;
    };

export interface SiteSettingsMixinInterface {
  delegate: ItemDelegate&SiteSettingsDelegate;
  enableEnhancedSiteControls: boolean;
  permittedSites: string[];
  restrictedSites: string[];
}
