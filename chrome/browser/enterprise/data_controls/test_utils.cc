// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_controls/test_utils.h"

#include "base/json/json_reader.h"
#include "components/enterprise/data_controls/prefs.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace data_controls {

void SetDataControls(PrefService* prefs, std::vector<std::string> rules) {
  ScopedListPrefUpdate list(prefs, kDataControlsRulesPref);
  if (!list->empty()) {
    list->clear();
  }

  for (const std::string& rule : rules) {
    list->Append(*base::JSONReader::Read(rule));
  }
}

}  // namespace data_controls
