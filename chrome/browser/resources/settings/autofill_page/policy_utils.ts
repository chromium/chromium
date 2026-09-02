// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ModelExecutionEnterprisePolicyValue} from '../ai_page/constants.js';
import {loadTimeData} from '../i18n_setup.js';

// LINT.IfChange(AutofillPolicyDataCategory)
export enum AutofillPolicyDataCategory {
  CONTACT_INFO = 'contact_info',
  PAYMENTS = 'payments',
  IDENTITY_DOCS = 'identity_docs',
  TRAVEL = 'travel',
  SHOPPING = 'shopping',
}
// LINT.ThenChange(//components/autofill/core/browser/permissions/autofill_policy_service.cc:AutofillPolicyDataCategory)

export interface TypesBlockedEntry {
  url_pattern: string;
  blocked_types: string[];
}

/**
 * Determines if a given Autofill data category (e.g. 'contact_info',
 * 'payments', 'identity_docs') is globally blocked across all sites by an
 * enterprise policy wildcard entry (`url_pattern: '*'`).
 */
export function isTypeGloballyBlocked(
    typesBlockedPref: chrome.settingsPrivate.PrefObject<TypesBlockedEntry[]>|
    undefined,
    category: AutofillPolicyDataCategory): boolean {
  if (!loadTimeData.valueExists('AutofillSettingsEnterprisePolicyEnabled') ||
      !loadTimeData.getBoolean('AutofillSettingsEnterprisePolicyEnabled')) {
    return false;
  }
  if (!typesBlockedPref || !typesBlockedPref.value ||
      !Array.isArray(typesBlockedPref.value)) {
    return false;
  }
  return typesBlockedPref.value.some(
      (entry: TypesBlockedEntry) => entry && entry.url_pattern === '*' &&
          entry.blocked_types && Array.isArray(entry.blocked_types) &&
          (entry.blocked_types.includes(category) ||
           entry.blocked_types.includes('all')));
}

/**
 * Computes an effective preference object for a boolean setting. If the given
 * category is globally blocked by enterprise policy (`autofill.types_blocked`),
 * returns a synthetic pref marked as ENFORCED and disabled (`value: false`).
 * Otherwise, returns the unmodified user preference.
 */
export function computeEffectiveAutofillPref(
    userPref: chrome.settingsPrivate.PrefObject<boolean>|undefined,
    typesBlockedPref: chrome.settingsPrivate.PrefObject<TypesBlockedEntry[]>|
    undefined,
    category: AutofillPolicyDataCategory):
    chrome.settingsPrivate.PrefObject<boolean>|undefined {
  if (!userPref) {
    return undefined;
  }

  const isBlocked = isTypeGloballyBlocked(typesBlockedPref, category);

  if (isBlocked) {
    return {
      ...userPref,
      value: false,
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
      controlledBy: chrome.settingsPrivate.ControlledBy.USER_POLICY,
    };
  }

  return {...userPref};
}

/**
 * Evaluates whether address autofill, general Autofill AI, or the wildcard
 * `autofill.types_blocked` policy applies to this Autofill AI entity category,
 * and sets the synthetic preference state (enforcement, value, controlledBy)
 * accordingly.
 */
export function checkAutofillPoliciesAndModifyPrefIfNecessary(
    pref: chrome.settingsPrivate.PrefObject<boolean>,
    addressPolicy: chrome.settingsPrivate.PrefObject<boolean>|undefined,
    autofillAiPolicy:
        chrome.settingsPrivate.PrefObject<ModelExecutionEnterprisePolicyValue>|
    undefined,
    typesBlockedPref: chrome.settingsPrivate.PrefObject<TypesBlockedEntry[]>|
    undefined,
    category: AutofillPolicyDataCategory) {
  // Due to the legacy AutofillAddressEnabled policy, if AutofillAddressEnabled
  // is disabled by an enterprise admin, Forms AI types (such as travel and
  // identity docs) are also disabled and enforced.
  if (addressPolicy?.enforcement ===
          chrome.settingsPrivate.Enforcement.ENFORCED &&
      !addressPolicy.value) {
    pref.enforcement = addressPolicy.enforcement;
    pref.controlledBy = addressPolicy.controlledBy;
    pref.value = addressPolicy.value;
    return;
  }
  if (autofillAiPolicy?.value === ModelExecutionEnterprisePolicyValue.DISABLE) {
    pref.enforcement = autofillAiPolicy.enforcement;
    pref.controlledBy = autofillAiPolicy.controlledBy;
    pref.value = false;
    return;
  }

  // If the wildcard AutofillSettings policy blocks this specific entity
  // category globally, enforce it and disable the toggle in Settings.
  const effectivePref =
      computeEffectiveAutofillPref(pref, typesBlockedPref, category);
  if (effectivePref) {
    pref.enforcement = effectivePref.enforcement;
    pref.controlledBy = effectivePref.controlledBy;
    pref.value = effectivePref.value;
  }
}
