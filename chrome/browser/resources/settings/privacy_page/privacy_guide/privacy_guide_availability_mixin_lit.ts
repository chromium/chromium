// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Contains utilities that track whether the Privacy Guide is available.
 */

import type {SyncStatus} from '/shared/settings/people_page/sync_browser_proxy.js';
import type {WebUiListenerMixinLitInterface} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {dedupingMixin} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {loadTimeData} from '../../i18n_setup.js';

type Constructor<T> = new (...args: any[]) => T;

export const PrivacyGuideAvailabilityMixinLit = dedupingMixin(
    <T extends Constructor<CrLitElement>>(superClass: T): T&
    Constructor<PrivacyGuideAvailabilityMixinLitInterface> => {
      const superClassBase = WebUiListenerMixinLit(superClass);

      class PrivacyGuideAvailabilityMixinLit extends superClassBase implements
          PrivacyGuideAvailabilityMixinLitInterface {
        static get properties() {
          return {
            isPrivacyGuideAvailable: {type: Boolean},
          };
        }

        accessor isPrivacyGuideAvailable: boolean =
            loadTimeData.getBoolean('showPrivacyGuide');

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
      return PrivacyGuideAvailabilityMixinLit;
    });

export interface PrivacyGuideAvailabilityMixinLitInterface extends
    WebUiListenerMixinLitInterface {
  isPrivacyGuideAvailable: boolean;
}
