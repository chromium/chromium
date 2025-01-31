// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import type {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {dedupingMixin} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {Setting, Settings} from '../data/model.js';
import {getInstance} from '../data/model.js';

type Constructor<T> = new (...args: any[]) => T;

export const SettingsMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<SettingsMixinInterface> => {
      class SettingsMixin extends superClass implements SettingsMixinInterface {
        static get properties() {
          return {
            settings: Object,
          };
        }

        settings: Settings;

        getSetting(settingName: keyof Settings): Setting {
          return getInstance().getSetting(settingName);
        }

        getSettingValue(settingName: keyof Settings): any {
          return getInstance().getSettingValue(settingName);
        }

        setSetting(
            settingName: keyof Settings, value: any, noSticky?: boolean) {
          getInstance().setSetting(settingName, value, noSticky);
        }

        setSettingSplice(
            settingName: keyof Settings, start: number, end: number,
            newValue: any, noSticky?: boolean) {
          getInstance().setSettingSplice(
              settingName, start, end, newValue, noSticky);
        }

        setSettingValid(settingName: keyof Settings, valid: boolean) {
          getInstance().setSettingValid(settingName, valid);
        }
      }

      return SettingsMixin;
    });

export interface SettingsMixinInterface {
  settings: Settings;

  /**
   * @param settingName Name of the setting to get.
   * @return The setting object.
   */
  getSetting(settingName: keyof Settings): Setting;

  /**
   * @param settingName Name of the setting to get the value for.
   * @return The value of the setting, accounting for availability.
   */
  getSettingValue(settingName: keyof Settings): any;

  /**
   * Sets settings.settingName.value to |value|, unless updating the setting is
   * disallowed by enterprise policy. Fires preview-setting-changed and
   * sticky-setting-changed events if the update impacts the preview or requires
   * an update to sticky settings.
   * @param settingName Name of the setting to set
   * @param value The value to set the setting to.
   * @param noSticky Whether to avoid stickying the setting. Defaults to false.
   */
  setSetting(settingName: keyof Settings, value: any, noSticky?: boolean): void;

  /**
   * @param settingName Name of the setting to set
   * @param newValue The value to add (if any).
   * @param noSticky Whether to avoid stickying the setting. Defaults to false.
   */
  setSettingSplice(
      settingName: keyof Settings, start: number, end: number, newValue: any,
      noSticky?: boolean): void;

  /**
   * Sets the validity of |settingName| to |valid|. If the validity is changed,
   * fires a setting-valid-changed event.
   * @param settingName Name of the setting to set
   * @param valid Whether the setting value is currently valid.
   */
  setSettingValid(settingName: keyof Settings, valid: boolean): void;
}
