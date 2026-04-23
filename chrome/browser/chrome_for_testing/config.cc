// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_for_testing/config.h"

#include <string_view>

#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/time/time.h"
#include "base/time/time_delta_from_string.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_for_testing/prefs.h"
#include "chrome/browser/chrome_for_testing/switches.h"
#include "components/component_updater/component_updater_paths.h"
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
constexpr char kRequiredComponents[] = "requiredComponents";
constexpr char kRequiredComponentsDir[] = "requiredComponentsDir";
constexpr char kRequiredComponentsUpdateTimeout[] =
    "requiredComponentsUpdateTimeout";
}  // namespace keys

// Required component attributes.
constexpr char kName[] = "name";
constexpr char kVersion[] = "version";

// Required components update timeout constrains.
constexpr base::TimeDelta kMinRequiredComponentsUpdateTimeout =
    base::Seconds(1);
constexpr base::TimeDelta kMaxRequiredComponentsUpdateTimeout =
    base::Minutes(5);

bool VerifyRequiredComponents(base::ListValue* required_components) {
  for (const auto& required_component : *required_components) {
    const base::DictValue* dict = required_component.GetIfDict();
    if (!dict) {
      LOG(ERROR) << "Required component entry is not an object: "
                 << required_component.DebugString();
      return false;
    }

    const std::string* name = dict->FindString(kName);
    if (!name || name->empty()) {
      LOG(ERROR) << "Missing required component name: "
                 << required_component.DebugString();
      return false;
    }

    if (dict->Find(kVersion)) {
      if (const std::string* version = dict->FindString(kVersion)) {
        if (!version->empty() && !base::Version(*version).IsValid()) {
          LOG(ERROR) << "Invalid required component version format: "
                     << required_component.DebugString();
          return false;
        }
      } else {
        LOG(ERROR) << "Invalid required component version type: "
                   << required_component.DebugString();
        return false;
      }
    }
  }
  return true;
}

bool VerifyRequiredComponentsDir(const base::FilePath& path) {
  if (path.empty()) {
    LOG(ERROR) << "Missing required components directory path";
    return false;
  }
  return true;
}

bool VerifyRequiredComponentsUpdateTimeout(
    std::optional<base::TimeDelta> timeout) {
  if (!timeout) {
    LOG(ERROR) << "Failed to parse required components update timeout string";
    return false;
  }
  if (*timeout < kMinRequiredComponentsUpdateTimeout) {
    LOG(ERROR) << "Required components update timeout cannot be less than "
               << kMinRequiredComponentsUpdateTimeout;
    return false;
  }
  if (*timeout > kMaxRequiredComponentsUpdateTimeout) {
    LOG(ERROR) << "Required components update timeout cannot be greater than "
               << kMaxRequiredComponentsUpdateTimeout;
    return false;
  }
  return true;
}

void OverrideRequiredComponentsDir(PrefService* pref_service) {
  base::FilePath path =
      pref_service->GetFilePath(prefs::kRequiredComponentsDir);
  if (path.empty()) {
    return;
  }
  base::PathService::Override(component_updater::DIR_COMPONENT_USER, path);
}

bool IsKnownKey(std::string_view key) {
  static const base::NoDestructor<base::flat_set<std::string_view>> kKnownKeys({
      keys::kEnableUserEducationUI,
      keys::kEnableSearchEngineChoiceDialog,
      keys::kEnableVirtualClipboard,
      keys::kRequiredComponents,
      keys::kRequiredComponentsDir,
      keys::kRequiredComponentsUpdateTimeout,
  });
  return kKnownKeys->contains(key);
}

}  // namespace

bool LoadConfig(PrefService* pref_service) {
  CHECK(pref_service);

  const base::CommandLine& command_line =
      CHECK_DEREF(base::CommandLine::ForCurrentProcess());

  // If there is no switch run with the default configuration.
  if (!command_line.HasSwitch(switches::kChromeForTestingConfig)) {
    ClearPrefs(pref_service);
    return true;
  }

  base::FilePath config_path =
      command_line.GetSwitchValuePath(switches::kChromeForTestingConfig);

  // If there is no configuration file specified run with the current
  // configuration. All configuration options except required components path
  // are already in effect. Required components path requires an explicit
  // override in the path service.
  if (config_path.empty()) {
    OverrideRequiredComponentsDir(pref_service);
    return true;
  }

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

  base::flat_set<std::string_view> recognized_keys;

  static constexpr struct {
    const char* json_key;
    const char* pref_name;
  } kConfigPrefMapping[] = {
      {keys::kEnableUserEducationUI, prefs::kEnableUserEducationUI},
      {keys::kEnableSearchEngineChoiceDialog,
       prefs::kEnableSearchEngineChoiceDialog},
      {keys::kEnableVirtualClipboard, prefs::kEnableVirtualClipboard},
  };
  for (const auto& mapping : kConfigPrefMapping) {
    if (auto value = dict->FindBool(mapping.json_key)) {
      pref_service->SetBoolean(mapping.pref_name, *value);
      recognized_keys.insert(mapping.json_key);
    }
  }

  if (base::ListValue* required_components =
          dict->FindList(keys::kRequiredComponents)) {
    if (!VerifyRequiredComponents(required_components)) {
      return false;
    }
    pref_service->SetList(prefs::kRequiredComponents,
                          std::move(*required_components));
    recognized_keys.insert(keys::kRequiredComponents);
  }

  if (const std::string* required_components_dir =
          dict->FindString(keys::kRequiredComponentsDir)) {
    const base::FilePath path =
        base::FilePath::FromUTF8Unsafe(*required_components_dir);
    if (!VerifyRequiredComponentsDir(path)) {
      return false;
    }
    pref_service->SetFilePath(prefs::kRequiredComponentsDir, path);
    OverrideRequiredComponentsDir(pref_service);
    recognized_keys.insert(keys::kRequiredComponentsDir);
  }

  if (const std::string* required_components_timeout =
          dict->FindString(keys::kRequiredComponentsUpdateTimeout)) {
    std::optional<base::TimeDelta> timeout =
        base::TimeDeltaFromString(*required_components_timeout);
    if (!VerifyRequiredComponentsUpdateTimeout(timeout)) {
      return false;
    }
    pref_service->SetTimeDelta(prefs::kRequiredComponentsUpdateTimeout,
                               *timeout);
    recognized_keys.insert(keys::kRequiredComponentsUpdateTimeout);
  }

  if (dict->size() > recognized_keys.size()) {
    LOG(ERROR) << "Found invalid keys in Chrome for Testing configuration "
                  "in file "
               << config_path.value();
    for (const auto [key, value] : *dict) {
      if (!recognized_keys.contains(key)) {
        if (IsKnownKey(key)) {
          LOG(ERROR) << "Invalid key value type: \"" << key << "\"";
        } else {
          LOG(ERROR) << "Unknown key: \"" << key << "\"";
        }
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

const base::ListValue& GetRequiredComponentsList() {
  const PrefService* pref_service = g_browser_process->local_state();
  return pref_service->GetList(prefs::kRequiredComponents);
}

base::FilePath GetRequiredComponentsDir() {
  const PrefService* pref_service = g_browser_process->local_state();
  return pref_service->GetFilePath(prefs::kRequiredComponentsDir);
}

base::TimeDelta GetRequiredComponentsUpdateTimeout() {
  const PrefService* pref_service = g_browser_process->local_state();
  return pref_service->GetTimeDelta(prefs::kRequiredComponentsUpdateTimeout);
}

base::flat_map<std::string, std::string> GetRequiredComponentsMap() {
  base::flat_map<std::string, std::string> required_components;
  for (const auto& required_component : GetRequiredComponentsList()) {
    const base::DictValue& dict = CHECK_DEREF(required_component.GetIfDict());
    const std::string& name = CHECK_DEREF(dict.FindString(kName));
    const std::string* version = dict.FindString(kVersion);
    required_components[name] = version ? *version : "";
  }

  return required_components;
}

}  // namespace chrome_for_testing
