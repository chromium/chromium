// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ash/app_list/arc/intent.h"

#include <cinttypes>
#include <string_view>

#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"

namespace arc {

namespace {
constexpr char kActionMain[] = "android.intent.action.MAIN";
}

constexpr char kInitialStartParam[] = "initialStart";
constexpr char kCategoryLauncher[] = "android.intent.category.LAUNCHER";
constexpr char kRequestStartTimeParamKey[] = "S.org.chromium.arc.request.start";
constexpr char kRequestDeferredStartTimeParamKey[] =
    "S.org.chromium.arc.request.deferred.start";
// Intent labels, kept in sorted order.
constexpr char kAction[] = "action";
constexpr char kCategory[] = "category";
constexpr char kComponent[] = "component";
constexpr char kEndSuffix[] = "end";
constexpr char kIntentPrefix[] = "#Intent";
constexpr char kLaunchFlags[] = "launchFlags";

Intent::Intent() = default;
Intent::~Intent() = default;

// static
std::unique_ptr<Intent> Intent::Get(const std::string& intent_as_string) {
  const std::vector<std::string_view> parts = base::SplitStringPiece(
      intent_as_string, ";", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (parts.size() < 2 || parts.front() != kIntentPrefix ||
      parts.back() != kEndSuffix) {
    DVLOG(1) << "Failed to split intent " << intent_as_string << ".";
    return nullptr;
  }

  auto intent = base::WrapUnique(new Intent());

  for (size_t i = 1; i < parts.size() - 1; ++i) {
    const size_t separator = parts[i].find('=');
    if (separator == std::string::npos) {
      if (parts[i].empty()) {
        // Intent should not have empty param. The empty param would appear in
        // intent string as ';;'. In the last case it would cause error in
        // Android framework. Such intents must not appear in the system.
        DVLOG(1) << "Found empty param in " << intent_as_string << ".";
        return nullptr;
      }
      intent->AddExtraParam(std::string(parts[i]));
      continue;
    }
    const std::string_view key = parts[i].substr(0, separator);
    const std::string_view value = parts[i].substr(separator + 1);
    if (key == kAction) {
      intent->set_action(std::string(value));
    } else if (key == kCategory) {
      intent->set_category(std::string(value));
    } else if (key == kLaunchFlags) {
      uint32_t launch_flags;
      const bool parsed = base::HexStringToUInt(value, &launch_flags);
      if (!parsed) {
        DVLOG(1) << "Failed to parse launchFlags: " << value << ".";
        return nullptr;
      }
      intent->set_launch_flags(launch_flags);
    } else if (key == kComponent) {
      const size_t component_separator = value.find('/');
      if (component_separator == std::string::npos)
        return nullptr;
      intent->set_package_name(
          std::string(value.substr(0, component_separator)));
      const std::string_view activity_compact_name =
          value.substr(component_separator + 1);
      if (!activity_compact_name.empty() && activity_compact_name[0] == '.') {
        std::string activity(value.substr(0, component_separator));
        activity += std::string(activity_compact_name);
        intent->set_activity(activity);
      } else {
        intent->set_activity(std::string(activity_compact_name));
      }
    } else {
      intent->AddExtraParam(std::string(parts[i]));
    }
  }

  return intent;
}

void Intent::AddExtraParam(const std::string& extra_param) {
  extra_params_.push_back(extra_param);
}

bool Intent::HasExtraParam(const std::string& extra_param) const {
  return base::Contains(extra_params_, extra_param);
}

bool Intent::GetExtraParamValue(const std::string& extra_param_key,
                                std::string* out) const {
  const std::string find_key = extra_param_key + "=";
  for (const auto& extra_param : extra_params()) {
    if (!extra_param.find(find_key)) {
      *out = extra_param.substr(find_key.length());
      return true;
    }
  }
  return false;
}

std::string GetLaunchIntent(const std::string& package_name,
                            const std::string& activity,
                            const std::vector<std::string>& extra_params) {
  const std::string extra_params_extracted =
      extra_params.empty() ? std::string()
                           : (base::JoinString(extra_params, ";") + ";");

  // Remove the |package_name| prefix, if activity starts with it.
  const char* activity_compact_name =
      activity.find(package_name.c_str()) == 0
          ? activity.c_str() + package_name.length()
          : activity.c_str();

  // Construct a string in format:
  // #Intent;action=android.intent.action.MAIN;
  //         category=android.intent.category.LAUNCHER;
  //         launchFlags=0x10210000;
  //         component=package_name/activity;
  //         param1;param2;end
  return base::StringPrintf(
      "%s;%s=%s;%s=%s;%s=0x%x;%s=%s/%s;%s%s", kIntentPrefix, kAction,
      kActionMain, kCategory, kCategoryLauncher, kLaunchFlags,
      Intent::FLAG_ACTIVITY_NEW_TASK |
          Intent::FLAG_ACTIVITY_RESET_TASK_IF_NEEDED,
      kComponent, package_name.c_str(), activity_compact_name,
      extra_params_extracted.c_str(), kEndSuffix);
}

std::string AppendLaunchIntent(const std::string& launch_intent,
                               const std::vector<std::string>& extra_params) {
  size_t insert_point = launch_intent.rfind(';');
  if (insert_point == std::string::npos) {
    LOG(ERROR) << "Failed to append invalid intent " << launch_intent;
    return launch_intent;
  }
  ++insert_point;

  const std::string end = launch_intent.substr(insert_point);
  if (end != kEndSuffix) {
    LOG(ERROR) << "Failed to append invalid intent " << launch_intent;
    return launch_intent;
  }

  const std::string extra_params_extracted =
      base::JoinString(extra_params, ";") + ';';
  return launch_intent.substr(0, insert_point) + extra_params_extracted + end;
}

std::string CreateIntentTicksExtraParam(const std::string& param_key,
                                        const base::TimeTicks& ticks) {
  return base::StringPrintf("%s=%" PRId64, param_key.c_str(),
                            (ticks - base::TimeTicks()).InMilliseconds());
}

}  // namespace arc
