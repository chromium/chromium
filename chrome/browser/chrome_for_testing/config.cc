// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_for_testing/config.h"

#include <string_view>

#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_for_testing/prefs.h"
#include "chrome/browser/chrome_for_testing/switches.h"
#include "components/prefs/pref_service.h"

using base::JSONParserOptions;
using base::JSONReader;

namespace chrome_for_testing {
namespace {

// Known configuration file keys.
namespace keys {
constexpr char kEnableUserEducationUI[] = "enableUserEducationUI";
constexpr char kEnableSearchEngineChoiceDialog[] =
    "enableSearchEngineChoiceDialog";
constexpr char kEnableVirtualClipboard[] = "enableVirtualClipboard";
}  // namespace keys
}  // namespace

bool LoadConfig(PrefService* pref_service) {
  CHECK(pref_service);

  const base::CommandLine& command_line =
      CHECK_DEREF(base::CommandLine::ForCurrentProcess());
  if (!command_line.HasSwitch(switches::kChromeForTestingConfig)) {
    return true;
  }

  base::FilePath config_path =
      command_line.GetSwitchValuePath(switches::kChromeForTestingConfig);
  std::string config_json;
  if (!base::ReadFileToString(config_path, &config_json)) {
    LOG(ERROR) << "Failed to read Chrome for Testing configuration file "
               << config_path.value();
    return false;
  }

  static constexpr int kJsonOptions =
      JSONParserOptions::JSON_ALLOW_COMMENTS |
      JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS;
  JSONReader::Result result =
      JSONReader::ReadAndReturnValueWithError(config_json, kJsonOptions);
  if (!result.has_value()) {
    LOG(ERROR) << "Failed to parse Chrome for Testing configuration in file "
               << config_path.value()
               << ", error: " << result.error().ToString();
    return false;
  }

  base::DictValue* dict = result.value().GetIfDict();
  if (!dict) {
    LOG(ERROR) << "Invalid Chrome for Testing configuration in file "
               << config_path.value() << ", it must be a JSON object.";
    return false;
  }

  static constexpr struct {
    const char* json_key;
    const char* pref_name;
  } kConfigPrefMapping[] = {
      {keys::kEnableUserEducationUI, prefs::kEnableUserEducationUI},
      {keys::kEnableSearchEngineChoiceDialog,
       prefs::kEnableSearchEngineChoiceDialog},
      {keys::kEnableVirtualClipboard, prefs::kEnableVirtualClipboard},
  };
  base::flat_set<std::string_view> recognized_keys;
  for (const auto& mapping : kConfigPrefMapping) {
    if (auto value = dict->FindBool(mapping.json_key)) {
      pref_service->SetBoolean(mapping.pref_name, *value);
      recognized_keys.insert(mapping.json_key);
    }
  }

  if (dict->size() > recognized_keys.size()) {
    LOG(ERROR) << "Found unrecognized keys in Chrome for Testing configuration "
                  "in file "
               << config_path.value();
    for (const auto [key, value] : *dict) {
      if (!recognized_keys.contains(key)) {
        LOG(ERROR) << "\"" << key << "\"";
      }
    }
    return false;
  }

  return true;
}

bool IsEnableUserEducationUI() {
  const PrefService* pref_service = g_browser_process->local_state();
  return pref_service->GetBoolean(prefs::kEnableUserEducationUI);
}

bool IsEnableSearchEngineChoiceDialog() {
  const PrefService* pref_service = g_browser_process->local_state();
  return pref_service->GetBoolean(prefs::kEnableSearchEngineChoiceDialog);
}

bool IsEnableVirtualClipboard() {
  const PrefService* pref_service = g_browser_process->local_state();
  return pref_service->GetBoolean(prefs::kEnableVirtualClipboard);
}

}  // namespace chrome_for_testing
