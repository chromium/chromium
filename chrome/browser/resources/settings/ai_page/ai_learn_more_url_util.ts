// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ModelExecutionEnterprisePolicyValue} from './constants.js';

export function getAiLearnMoreUrl(
    enterprisePref: chrome.settingsPrivate.PrefObject, learnMoreUrl: string,
    learnMoreEnterpriseUrl: string): string {
  return enterprisePref.value ===
          ModelExecutionEnterprisePolicyValue.ALLOW_WITHOUT_LOGGING ?
      learnMoreEnterpriseUrl :
      learnMoreUrl;
}
