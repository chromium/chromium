// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Keep in sync with GetChoiceListJSON in the backend.
// LINT.IfChange
export interface SearchEngineChoice {
  prepopulateId: number;
  name: string;
  iconPath: string;
  url: string;
  marketingSnippet: string;
  showMarketingSnippet: boolean;
}
// LINT.ThenChange(/chrome/browser/ui/webui/search_engine_choice/search_engine_choice_ui.cc)
