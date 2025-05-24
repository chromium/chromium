// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import type {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {Setting, Settings} from '../data/model.js';
import {getInstance} from '../data/model.js';
import type {PrintPreviewModelElement} from '../data/model.js';
import type {ChangeCallback} from '../data/observable.js';

type Constructor<T> = new (...args: any[]) => T;

export const SettingsMixin = <T extends Constructor<CrLitElement>>(
    superClass: T): T&Constructor<SettingsMixinInterface> => {
  class SettingsMixin extends superClass implements SettingsMixinInterface {
    private observers_: number[] = [];
    private model_: PrintPreviewModelElement|null = null;

    override connectedCallback() {
      super.connectedCallback();
      // Cache this reference, so that the same one can be used in
      // disconnectedCallback(), othehrwise if `model_` has already been removed
      // from the DOM, getInstance() will throw an error.
      this.model_ = getInstance();
    }

    override disconnectedCallback() {
      super.disconnectedCallback();
      assert(this.model_);

      if (this.model_.isConnected) {
        // Only remove observers if the PrintPreviewModelElement original
        // singleton instance is still connected to the DOM. Otherwise all
        // observers have already been remomved in PrintPreviewModelElement's
        // disconnectedCallback.
        for (const id of this.observers_) {
          const removed = this.model_.observable.removeObserver(id);
          assert(removed);
        }
      }

      this.model_ = null;
      this.observers_ = [];
    }

    addSettingObserver(path: string, callback: ChangeCallback) {
      const id = getInstance().observable.addObserver(path, callback);
      this.observers_.push(id);
    }

    getSetting(settingName: keyof Settings): Setting {
      return getInstance().getSetting(settingName);
    }

    getSettingValue(settingName: keyof Settings): any {
      return getInstance().getSettingValue(settingName);
    }

    setSetting(settingName: keyof Settings, value: any, noSticky?: boolean) {
      getInstance().setSetting(settingName, value, noSticky);
    }

    setSettingValid(settingName: keyof Settings, valid: boolean) {
      getInstance().setSettingValid(settingName, valid);
    }
  }

  return SettingsMixin;
};

export interface SettingsMixinInterface {
  addSettingObserver(path: string, callback: ChangeCallback): void;

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
   * Sets the validity of |settingName| to |valid|. If the validity is changed,
   * fires a setting-valid-changed event.
   * @param settingName Name of the setting to set
   * @param valid Whether the setting value is currently valid.
   */
  setSettingValid(settingName: keyof Settings, valid: boolean): void;
}
