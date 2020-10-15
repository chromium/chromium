// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';

import {PasswordManagerImpl, PasswordManagerProxy} from './password_manager_proxy.js';

/**
 * This behavior bundles functionality required to get insecure credentials and
 * status of password check. It is used by <settings-password-check>
 * <passwords-section> and <settings-autofill-page>.
 *
 * @polymerBehavior
 */

export const PasswordCheckBehavior = {

  properties: {
    /**
     * The number of compromised passwords as a formatted string.
     */
    compromisedPasswordsCount: String,

    /**
     * The number of weak passwords as a formatted string.
     */
    weakPasswordsCount: String,

    /**
     * The number of insecure passwords as a formatted string.
     */
    insecurePasswordsCount: String,

    /**
     * An array of leaked passwords to display.
     * @type {!Array<!PasswordManagerProxy.InsecureCredential>}
     */
    leakedPasswords: {
      type: Array,
      value: () => [],
    },

    /**
     * An array of weak passwords to display.
     * @type {!Array<!PasswordManagerProxy.InsecureCredential>}
     */
    weakPasswords: {
      type: Array,
      value: () => [],
    },

    /**
     * The status indicates progress and affects banner, title and icon.
     * @type {!PasswordManagerProxy.PasswordCheckStatus}
     */
    status: {
      type: Object,
      value: () => ({state: chrome.passwordsPrivate.PasswordCheckState.IDLE}),
    },

    /**
     * Stores whether the status was fetched from the backend.
     * @type {boolean}
     */
    isInitialStatus: {
      type: Boolean,
      value: true,
    },

    /**
     * Returns true if passwords weakness check is enabled.
     * @type {boolean}
     */
    passwordsWeaknessCheckEnabled: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('passwordsWeaknessCheck');
      }
    },
  },

  /**
   * @private {?function(!PasswordManagerProxy.InsecureCredentials):void}
   */
  leakedCredentialsListener_: null,

  /**
   * @private {?function(!PasswordManagerProxy.InsecureCredentials):void}
   */
  weakCredentialsListener_: null,

  /**
   * @private {?function(!PasswordManagerProxy.PasswordCheckStatus):void}
   */
  statusChangedListener_: null,

  /** @type {?PasswordManagerProxy} */
  passwordManager: null,

  /** @override */
  attached() {
    this.statusChangedListener_ = status => {
      this.status = status;
      this.isInitialStatus = false;
    };

    this.leakedCredentialsListener_ = compromisedCredentials => {
      this.updateCompromisedPasswordList(compromisedCredentials);
      this.fetchPluralizedStrings_();
    };

    this.weakCredentialsListener_ = weakCredentials => {
      this.weakPasswords = weakCredentials;
      this.fetchPluralizedStrings_();
    };

    this.passwordManager = PasswordManagerImpl.getInstance();
    this.passwordManager.getPasswordCheckStatus().then(
        this.statusChangedListener_);
    this.passwordManager.getCompromisedCredentials().then(
        this.leakedCredentialsListener_);
    this.passwordManager.getWeakCredentials().then(
        this.weakCredentialsListener_);

    this.passwordManager.addPasswordCheckStatusListener(
        this.statusChangedListener_);
    this.passwordManager.addCompromisedCredentialsListener(
        this.leakedCredentialsListener_);
    this.passwordManager.addWeakCredentialsListener(
        this.weakCredentialsListener_);
  },

  /** @override */
  detached() {
    this.passwordManager.removeCompromisedCredentialsListener(
        assert(this.statusChangedListener_));
    this.statusChangedListener_ = null;
    this.passwordManager.removeCompromisedCredentialsListener(
        assert(this.leakedCredentialsListener_));
    this.leakedCredentialsListener_ = null;
    this.passwordManager.removeWeakCredentialsListener(
        assert(this.weakCredentialsListener_));
    this.weakCredentialsListener_ = null;
  },

  /**
   * Helper that fetches pluralized strings corresponding to the number of
   * compromised, weak and insecure credentials.
   * @private
   */
  fetchPluralizedStrings_() {
    const proxy = PluralStringProxyImpl.getInstance();
    const compromised = this.leakedPasswords.length;
    const weak = this.weakPasswords.length;

    proxy.getPluralString('compromisedPasswords', compromised)
        .then(count => this.compromisedPasswordsCount = count);

    proxy.getPluralString('weakPasswords', weak)
        .then(count => this.weakPasswordsCount = count);

    proxy.getPluralString('insecurePasswords', compromised + weak)
        .then(count => this.insecurePasswordsCount = count);
  },

  /**
   * Function to update compromised credentials in a proper way. New entities
   * should appear in the bottom.
   * @param {!Array<!PasswordManagerProxy.InsecureCredential>} newList
   * @private
   */
  updateCompromisedPasswordList(newList) {
    const oldList = this.leakedPasswords.slice();
    const map = new Map(newList.map(item => ([item.id, item])));

    const resultList = [];

    for (const item of oldList) {
      // If element is present in newList
      if (map.has(item.id)) {
        // Replace old version with new
        resultList.push(map.get(item.id));
        map.delete(item.id);
      }
    }

    const addedResults = Array.from(map.values());
    addedResults.sort((lhs, rhs) => {
      // Phished passwords are always shown above leaked passwords.
      const isPhished = cred => cred.compromisedInfo.compromiseType !==
          chrome.passwordsPrivate.CompromiseType.LEAKED;
      if (isPhished(lhs) !== isPhished(rhs)) {
        return isPhished(lhs) ? -1 : 1;
      }

      // Sort by time only if the displayed elapsed time since compromise is
      // different.
      if (lhs.compromisedInfo.elapsedTimeSinceCompromise !==
          rhs.compromisedInfo.elapsedTimeSinceCompromise) {
        return rhs.compromisedInfo.compromiseTime -
            lhs.compromisedInfo.compromiseTime;
      }

      // Otherwise sort by origin, or by username in case the origin is the
      // same.
      return lhs.formattedOrigin.localeCompare(rhs.formattedOrigin) ||
          lhs.username.localeCompare(rhs.username);
    });
    resultList.push(...addedResults);
    this.leakedPasswords = resultList;
  },
};
