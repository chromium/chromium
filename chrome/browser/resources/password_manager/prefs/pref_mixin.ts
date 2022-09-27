// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert_ts.js';
import {dedupingMixin, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PrefsBrowserProxyImpl, PrefsChangedListener} from './prefs_browser_proxy.js';

type Constructor<T> = new (...args: any[]) => T;

export const PrefMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<PrefMixinInterface> => {
      class PrefMixin extends superClass implements PrefMixinInterface {
        static get properties() {
          return {
            prefKey: String,

            pref: {
              type: Object,
              notify: true,
              value: null,
            },
          };
        }

        prefKey: string;
        pref: chrome.settingsPrivate.PrefObject|null;
        private prefsChanged_: PrefsChangedListener|null = null;

        override connectedCallback() {
          super.connectedCallback();

          assert(this.prefKey);

          this.prefsChanged_ = this.onPrefsChangedInSettingsPrivate_.bind(this);
          PrefsBrowserProxyImpl.getInstance().addPrefsChangedListener(
              this.prefsChanged_);
          this.refresh_();
        }

        override disconnectedCallback() {
          super.disconnectedCallback();

          assert(this.prefsChanged_);
          PrefsBrowserProxyImpl.getInstance().removePrefsChangedListener(
              this.prefsChanged_);
          this.prefsChanged_ = null;
        }

        private onPrefsChangedInSettingsPrivate_(
            prefs: chrome.settingsPrivate.PrefObject[]) {
          const pref = prefs.find(i => i.key === this.prefKey);

          if (pref) {
            this.pref = pref;
          }
        }

        setPrefValue(value: boolean) {
          assert(this.pref);

          if (value === this.pref.value) {
            return;
          }

          PrefsBrowserProxyImpl.getInstance().setPref(this.prefKey, value);
        }

        private async refresh_() {
          this.pref =
              await PrefsBrowserProxyImpl.getInstance().getPref(this.prefKey);
          assert(this.pref);
          assert(this.pref.type === chrome.settingsPrivate.PrefType.BOOLEAN);
        }
      }

      return PrefMixin;
    });

export interface PrefMixinInterface {
  prefKey: string;
  pref: chrome.settingsPrivate.PrefObject|null;
  setPrefValue(value: boolean): void;
}
