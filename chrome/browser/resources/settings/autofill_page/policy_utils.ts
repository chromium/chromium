// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ModelExecutionEnterprisePolicyValue} from '../ai_page/constants.js';

/**
 * Checks if the address autofill is enforced by policy and modifies the fake
 * preference accordingly.
 */
function checkAddressPolicyAndModifyPrefIfNecessary(
    pref: chrome.settingsPrivate.PrefObject<boolean>,
    addressPolicy: chrome.settingsPrivate.PrefObject<boolean>): boolean {
  if (addressPolicy.enforcement ===
          chrome.settingsPrivate.Enforcement.ENFORCED &&
      !addressPolicy.value) {
    pref.enforcement = addressPolicy.enforcement;
    pref.controlledBy = addressPolicy.controlledBy;
    pref.value = addressPolicy.value;
    return true;
  }
  return false;
}

/**
 * Checks if the Autofill AI feature is disabled by policy and modifies the fake
 * preference accordingly.
 */
function checkAutofillAiPolicyAndModifyPrefIfNecessary(
    pref: chrome.settingsPrivate.PrefObject<boolean>,
    autofillAiPolicy:
        chrome.settingsPrivate.PrefObject<ModelExecutionEnterprisePolicyValue>):
    boolean {
  if (autofillAiPolicy.value === ModelExecutionEnterprisePolicyValue.DISABLE) {
    pref.enforcement = autofillAiPolicy.enforcement;
    pref.controlledBy = autofillAiPolicy.controlledBy;
    pref.value = false;
    return true;
  }
  return false;
}

/**
 * Checks if the address autofill or the Autofill AI feature are disabled by
 * policy and modifies the fake preference accordingly.
 */
export function checkAutofillPoliciesAndModifyPrefIfNecessary(
    pref: chrome.settingsPrivate.PrefObject<boolean>,
    addressPolicy: chrome.settingsPrivate.PrefObject<boolean>,
    autofillAiPolicy: chrome.settingsPrivate
        .PrefObject<ModelExecutionEnterprisePolicyValue>) {
  const addressPolicyIsActive =
      checkAddressPolicyAndModifyPrefIfNecessary(pref, addressPolicy);
  if (!addressPolicyIsActive) {
    checkAutofillAiPolicyAndModifyPrefIfNecessary(pref, autofillAiPolicy);
  }
}
