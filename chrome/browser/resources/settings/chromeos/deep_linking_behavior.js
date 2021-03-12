// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * @fileoverview Polymer behavior for scrolling/focusing/indicating
 * setting elements with deep links.
 */

// clang-format off
// #import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js'
// #import '../constants/setting.mojom-lite.js';

// #import {afterNextRender, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {assert} from 'chrome://resources/js/assert.m.js';
// #import {getSettingIdParameter} from '../setting_id_param_util.js';
// #import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
// #import {Router} from '../router.js';
// clang-format on

/** @type {string} */
const kDeepLinkFocusId = 'deep-link-focus-id';

/** @polymerBehavior */
/* #export */ const DeepLinkingBehavior = {
  properties: {
    /**
     * An object whose values are the kSetting mojom values which can be used to
     * define deep link IDs on HTML elems.
     * @type {!Object}
     */
    Setting: {
      type: Object,
      value: chromeos.settings.mojom.Setting,
    },

    /**
     * Set of setting IDs that could be deep linked to. Initialized as an
     * empty set, should be overridden with applicable setting IDs.
     * @type {!Set<!chromeos.settings.mojom.Setting>}
     */
    supportedSettingIds: {
      type: Object,
      value: () => new Set(),
    },
  },

  /**
   * Retrieves the settingId saved in the url's query parameter. Returns null if
   * deep linking is disabled or if no settingId is found.
   * @return {?chromeos.settings.mojom.Setting}
   */
  getDeepLinkSettingId() {
    const settingIdStr = getSettingIdParameter();
    if (!settingIdStr) {
      return null;
    }
    const settingIdNum = Number(settingIdStr);
    if (isNaN(settingIdNum)) {
      return null;
    }
    return /** @type {!chromeos.settings.mojom.Setting} */ (settingIdNum);
  },

  /**
   * Focuses the deep linked element referred to by |settingId|. Returns a
   * Promise for an object that reflects if the deep link was shown or not. The
   * object has a boolean |deepLinkShown| and any |pendingSettingId| that
   * couldn't be shown.
   * @param {!chromeos.settings.mojom.Setting} settingId
   * @return {!Promise<!{deepLinkShown: boolean, pendingSettingId:
   *     ?chromeos.settings.mojom.Setting}>}
   */
  showDeepLink(settingId) {
    assert(loadTimeData.getBoolean('isDeepLinkingEnabled'));

    return new Promise(resolve => {
      Polymer.RenderStatus.afterNextRender(this, () => {
        const elToFocus = this.$$(`[${kDeepLinkFocusId}~="${settingId}"]`);
        if (!elToFocus || elToFocus.hidden) {
          console.warn(`Element with deep link id ${settingId} not focusable.`);
          resolve({deepLinkShown: false, pendingSettingId: settingId});
          return;
        }

        this.showDeepLinkElement(elToFocus);
        resolve({deepLinkShown: true, pendingSettingId: settingId});
      });
    });
  },

  /**
   * Focuses the deep linked element |elem|. Returns whether the deep link was
   * shown or not.
   * @param {!Element} elToFocus
   */
  showDeepLinkElement(elToFocus) {
    assert(loadTimeData.getBoolean('isDeepLinkingEnabled'));

    elToFocus.focus();
  },

  /**
   * Override this method to execute code after a supported settingId is found
   * and before the deep link is shown. Returns whether or not the deep link
   * attempt should continue. Default behavior is to no op and then return
   * true, continuing the deep link attempt.
   * @param {!chromeos.settings.mojom.Setting} settingId
   * @return {boolean}
   */
  beforeDeepLinkAttempt(settingId) {
    return true;
  },

  /**
   * Checks if there are settingIds that can be linked to and attempts to show
   * the deep link. Returns a Promise for an object that reflects if the deep
   * link was shown or not. The object has a boolean |deepLinkShown| and any
   * |pendingSettingId| that couldn't be shown.
   * @return {!Promise<!{deepLinkShown: boolean, pendingSettingId:
   *     ?chromeos.settings.mojom.Setting}>}
   */
  attemptDeepLink() {
    const settingId = this.getDeepLinkSettingId();
    // Explicitly check for null to handle settingId = 0.
    if (settingId === null || !this.supportedSettingIds.has(settingId)) {
      // No deep link was shown since the settingId was unsupported.
      return new Promise(resolve => {
        resolve({deepLinkShown: false, pendingSettingId: null});
      });
    }

    const shouldContinue = this.beforeDeepLinkAttempt(settingId);
    if (!shouldContinue) {
      // Don't continue the deep link attempt since it was presumably
      // handled manually in beforeDeepLinkAttempt().
      return new Promise(resolve => {
        resolve({deepLinkShown: false, pendingSettingId: settingId});
      });
    }
    return this.showDeepLink(settingId);
  },
};
