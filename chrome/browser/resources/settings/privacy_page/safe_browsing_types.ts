// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Enumeration of all safe browsing modes. Must be kept in sync with the enum
 * of the same name located in:
 * components/safe_browsing/core/common/safe_browsing_prefs.h
 */
// LINT.IfChange(SafeBrowsingSetting)
export enum SafeBrowsingSetting {
  DISABLED = 0,
  STANDARD = 1,
  ENHANCED = 2,
}
// LINT.ThenChange(/components/safe_browsing/core/common/safe_browsing_prefs.h:SafeBrowsingState)
