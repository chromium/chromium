// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Behavior common to Site Settings classes.
 */

// clang-format off
import type { PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {dedupingMixin} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {ContentSettingsTypes} from './constants.js';
import {ContentSetting, SiteSettingSource} from './constants.js';
import type {RawSiteException,SiteException,SiteSettingsPrefsBrowserProxy} from './site_settings_prefs_browser_proxy.js';
import {SiteSettingsPrefsBrowserProxyImpl} from './site_settings_prefs_browser_proxy.js';
// clang-format on

type Constructor<T> = new (...args: any[]) => T;

export const SiteSettingsMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<SiteSettingsMixinInterface> => {
      class SiteSettingsMixin extends superClass {
        static get properties() {
          return {
            /**
             * The string ID of the category this element is displaying data
             * for. See site_settings/constants.js for possible values.
             */
            category: String,

            /**
             * A cached list of ContentSettingsTypes with a standard
             * allow-block-ask pattern that are currently enabled for use. This
             * property is the same across all elements with SiteSettingsMixin
             * ('static').
             */
            contentTypes_: {
              type: Array,
              value: [],
            },
          };
        }

        category: ContentSettingsTypes;
        private contentTypes_: ContentSettingsTypes[];
        browserProxy: SiteSettingsPrefsBrowserProxy;

        constructor(...args: any[]) {
          super(...args);

          /**
           * The browser proxy used to retrieve and change information about
           * site settings categories and the sites within.
           */
          this.browserProxy = SiteSettingsPrefsBrowserProxyImpl.getInstance();
        }

        /**
         * Ensures the URL has a scheme (assumes http if omitted).
         * @param url The URL with or without a scheme.
         * @return The URL with a scheme, or an empty string.
         */
        ensureUrlHasScheme(url: string): string {
          if (url.length === 0) {
            return url;
          }
          return url.includes('://') ? url : 'http://' + url;
        }

        /**
         * Removes redundant ports, such as port 80 for http and 443 for https.
         * @param url The URL to sanitize.
         * @return The URL without redundant ports, if any.
         */
        sanitizePort(url: string): string {
          const urlWithScheme = this.ensureUrlHasScheme(url);
          if (urlWithScheme.startsWith('https://') &&
              urlWithScheme.endsWith(':443')) {
            return url.slice(0, -4);
          }
          if (urlWithScheme.startsWith('http://') &&
              urlWithScheme.endsWith(':80')) {
            return url.slice(0, -3);
          }
          return url;
        }

        /**
         * @return true if the passed content setting is considered 'enabled'.
         */
        computeIsSettingEnabled(setting: ContentSetting): boolean {
          return setting !== ContentSetting.BLOCK;
        }

        /**
         * Converts a string origin/pattern to a URL.
         * @param originOrPattern The origin/pattern to convert to URL.
         * @return The URL to return (or null if origin is not a valid URL).
         */
        toUrl(originOrPattern: string): URL|null {
          if (originOrPattern.length === 0) {
            return null;
          }
          // TODO(finnur): Hmm, it would probably be better to ensure scheme on
          // the JS/C++ boundary.
          // TODO(dschuyler): I agree. This filtering should be done in one go,
          // rather that during the sort. The URL generation should be wrapped
          // in a try/catch as well.
          originOrPattern = originOrPattern.replace('*://', '');
          originOrPattern = originOrPattern.replace('[*.]', '');
          return new URL(this.ensureUrlHasScheme(originOrPattern));
        }

        /**
         * @return a user-friendly name for the origin.
         */
        originRepresentation(origin: string): string {
          try {
            const url = this.toUrl(origin);
            return url ? (url.host || url.origin) : '';
          } catch (error) {
            return '';
          }
        }

        /**
         * Convert an exception (received from the C++ handler) to a full
         * SiteException.
         * @param exception The raw site exception from C++.
         * @return The expanded (full) SiteException.
         */
        expandSiteException(exception: RawSiteException): SiteException {
          const origin = exception.origin;
          const embeddingOrigin = exception.embeddingOrigin;

          // TODO(patricialor): |exception.source| should be one of the values
          // defined in |SiteSettingSource|.
          let enforcement = null;
          if (exception.source === SiteSettingSource.EXTENSION ||
              exception.source === SiteSettingSource.HOSTED_APP ||
              exception.source === SiteSettingSource.POLICY) {
            enforcement = chrome.settingsPrivate.Enforcement.ENFORCED;
          }

          let controlledBy = chrome.settingsPrivate.ControlledBy.PRIMARY_USER;
          if (exception.source === SiteSettingSource.EXTENSION ||
              exception.source === SiteSettingSource.HOSTED_APP) {
            controlledBy = chrome.settingsPrivate.ControlledBy.EXTENSION;
          } else if (exception.source === SiteSettingSource.POLICY) {
            controlledBy = chrome.settingsPrivate.ControlledBy.USER_POLICY;
          }

          return {
            category: exception.type as ContentSettingsTypes,
            embeddingOrigin: embeddingOrigin,
            incognito: exception.incognito,
            isEmbargoed: exception.isEmbargoed,
            origin: origin,
            displayName: exception.displayName,
            setting: exception.setting,
            description: exception.description,
            enforcement: enforcement,
            controlledBy: controlledBy,
          };
        }
      }

      return SiteSettingsMixin;
    });

export interface SiteSettingsMixinInterface {
  browserProxy: SiteSettingsPrefsBrowserProxy;
  category: ContentSettingsTypes;
  computeIsSettingEnabled(setting: string): boolean;
  originRepresentation(origin: string): string;
  toUrl(originOrPattern: string): URL|null;
  expandSiteException(exception: RawSiteException): SiteException;
  sanitizePort(url: string): string;
}
