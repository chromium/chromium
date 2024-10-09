// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides behavior to fetch the list of user specified permitted
 * and restricted sites on creation and when these lists are updated. Used by
 * multiple pages.
 */

import type {CrLitElement, PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import type {ItemDelegate} from '../item.js';

import type {SiteSettingsDelegate} from './site_settings_mixin.js';
import {DummySiteSettingsMixinDelegate} from './site_settings_mixin.js';

type Constructor<T> = new (...args: any[]) => T;

export const SiteSettingsMixinLit =
    <T extends Constructor<CrLitElement>>(superClass: T): T&
    Constructor<SiteSettingsMixinLitInterface> => {
      class SiteSettingsMixinLit extends superClass implements
          SiteSettingsMixinLitInterface {
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

      return SiteSettingsMixinLit;
    };

export interface SiteSettingsMixinLitInterface {
  delegate: ItemDelegate&SiteSettingsDelegate;
  enableEnhancedSiteControls: boolean;
  permittedSites: string[];
  restrictedSites: string[];
}
