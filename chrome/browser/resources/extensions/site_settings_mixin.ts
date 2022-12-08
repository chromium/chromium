// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides behavior to fetch the list of user specified permitted
 * and restricted sites on creation and when these lists are updated. Used by
 * multiple pages.
 */

import {ChromeEvent} from '/tools/typescript/definitions/chrome_event.js';
import {dedupingMixin, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ItemDelegate} from './item.js';

type Constructor<T> = new (...args: any[]) => T;

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

export const SiteSettingsMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<SiteSettingsMixinInterface> => {
      class SiteSettingsMixin extends superClass {
        static get properties() {
          return {
            delegate: Object,
            enableEnhancedSiteControls: Boolean,

            restrictedSites: {
              type: Array,
              value: [],
            },

            permittedSites: {
              type: Array,
              value: [],
            },
          };
        }

        delegate: ItemDelegate&SiteSettingsDelegate;
        enableEnhancedSiteControls: boolean;
        restrictedSites: string[];
        protected permittedSites: string[];

        override ready() {
          super.ready();
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
    });

export interface SiteSettingsMixinInterface {
  delegate: ItemDelegate&SiteSettingsDelegate;
  enableEnhancedSiteControls: boolean;
  restrictedSites: string[];
}
