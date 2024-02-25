// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Contains utilities that track whether the Privacy Guide is available.
 */

import type {SyncStatus} from '/shared/settings/people_page/sync_browser_proxy.js';
import type {WebUiListenerMixinInterface} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import type {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {dedupingMixin} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../../i18n_setup.js';

type Constructor<T> = new (...args: any[]) => T;

export interface PrivacyGuideAvailabilityMixinInterface extends
    WebUiListenerMixinInterface {
  isPrivacyGuideAvailable: boolean;
}

export const PrivacyGuideAvailabilityMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<PrivacyGuideAvailabilityMixinInterface> => {
      const superClassBase = WebUiListenerMixin(superClass);

      class PrivacyGuideAvailabilityMixinInternal extends superClassBase
          implements PrivacyGuideAvailabilityMixinInterface {
        static get properties() {
          return {
            isPrivacyGuideAvailable: {
              type: Boolean,
              value: () => loadTimeData.getBoolean('showPrivacyGuide'),
            },
          };
        }

        isPrivacyGuideAvailable: boolean;

        override connectedCallback(): void {
          super.connectedCallback();

          this.addWebUiListener(
              'is-managed-changed',
              (isManaged: boolean) =>
                  this.onPrivacyGuideAvailabilityChanged_(!isManaged));
          this.addWebUiListener(
              'sync-status-changed',
              (syncStatus: SyncStatus) =>
                  this.onPrivacyGuideAvailabilityChanged_(
                      !syncStatus.supervisedUser));
        }

        private onPrivacyGuideAvailabilityChanged_(isAvailable: boolean) {
          // If the Privacy Guide becomes unavailable, then hide the entry
          // point. However, if the Privacy Guide was unavailable before, but
          // now is, then do not make the privacy guide entry point visible,
          // as the Settings route for privacy guide would still be unavailable
          // until the page is reloaded.
          this.isPrivacyGuideAvailable =
              this.isPrivacyGuideAvailable && isAvailable;
        }
      }
      return PrivacyGuideAvailabilityMixinInternal;
    });
