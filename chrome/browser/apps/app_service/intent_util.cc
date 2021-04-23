// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/intent_util.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/apps/app_service/file_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/chrome_features.h"
#include "components/services/app_service/public/cpp/intent_filter_util.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "components/services/app_service/public/mojom/types.mojom.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "components/arc/intent_helper/intent_constants.h"
#include "components/arc/mojom/intent_helper.mojom.h"
#endif

namespace {

constexpr char kTextPlain[] = "text/plain";

apps::mojom::IntentFilterPtr CreateShareFileFilter(
    const std::vector<std::string>& intent_actions,
    const std::vector<std::string>& content_types) {
  DCHECK(!content_types.empty());
  auto intent_filter = apps::mojom::IntentFilter::New();

  std::vector<apps::mojom::ConditionValuePtr> action_condition_values;
  for (auto& action : intent_actions) {
    action_condition_values.push_back(apps_util::MakeConditionValue(
        action, apps::mojom::PatternMatchType::kNone));
  }
  if (!action_condition_values.empty()) {
    auto action_condition =
        apps_util::MakeCondition(apps::mojom::ConditionType::kAction,
                                 std::move(action_condition_values));
    intent_filter->conditions.push_back(std::move(action_condition));
  }

  std::vector<apps::mojom::ConditionValuePtr> mime_type_condition_values;
  for (auto& mime_type : content_types) {
    mime_type_condition_values.push_back(apps_util::MakeConditionValue(
        mime_type, apps::mojom::PatternMatchType::kMimeType));
  }
  if (!mime_type_condition_values.empty()) {
    auto mime_type_condition =
        apps_util::MakeCondition(apps::mojom::ConditionType::kMimeType,
                                 std::move(mime_type_condition_values));
    intent_filter->conditions.push_back(std::move(mime_type_condition));
  }

  return intent_filter;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
constexpr char kIntentExtraText[] = "S.android.intent.extra.TEXT";
constexpr char kIntentExtraSubject[] = "S.android.intent.extra.SUBJECT";
constexpr char kIntentExtraStartType[] = "S.org.chromium.arc.start_type";
constexpr char kIntentActionPrefix[] = "android.intent.action";
constexpr char kType[] = "type";

constexpr int kIntentPrefixLength = 2;

const char* GetArcIntentAction(const std::string& action) {
  if (action == apps_util::kIntentActionMain) {
    return arc::kIntentActionMain;
  } else if (action == apps_util::kIntentActionView) {
    return arc::kIntentActionView;
  } else if (action == apps_util::kIntentActionSend) {
    return arc::kIntentActionSend;
  } else if (action == apps_util::kIntentActionSendMultiple) {
    return arc::kIntentActionSendMultiple;
  } else if (action.compare(0, strlen(kIntentActionPrefix),
                            kIntentActionPrefix) == 0) {
    return action.c_str();
  } else {
    return arc::kIntentActionView;
  }
}
#endif

}  // namespace

namespace apps_util {

void PopulateWebAppIntentFilters(
    const web_app::WebApp& web_app,
    std::vector<apps::mojom::IntentFilterPtr>& target) {
  if (web_app.scope().is_empty())
    return;

  target.push_back(apps_util::CreateIntentFilterForUrlScope(
      web_app.scope(),
      base::FeatureList::IsEnabled(features::kIntentHandlingSharing)));

  if (!base::FeatureList::IsEnabled(features::kIntentHandlingSharing) ||
      !web_app.share_target().has_value()) {
    return;
  }

  const apps::ShareTarget& share_target = web_app.share_target().value();

  if (!share_target.params.text.empty()) {
    // The share target accepts navigator.share() calls with text.
    target.push_back(
        CreateShareFileFilter({apps_util::kIntentActionSend}, {kTextPlain}));
  }

  std::vector<std::string> content_types;
  for (const auto& files_entry : share_target.params.files) {
    for (const auto& file_type : files_entry.accept) {
      // Skip any file_type that is not a MIME type.
      if (file_type.empty() || file_type[0] == '.' ||
          std::count(file_type.begin(), file_type.end(), '/') != 1) {
        continue;
      }

      content_types.push_back(file_type);
    }
  }

  if (!content_types.empty()) {
    const std::vector<std::string> intent_actions(
        {apps_util::kIntentActionSend, apps_util::kIntentActionSendMultiple});
    target.push_back(CreateShareFileFilter(intent_actions, content_types));
  }
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// TODO(crbug.com/853604): Make this not link to file manager extension if
// possible.
apps::mojom::IntentPtr CreateShareIntentFromFiles(
    Profile* profile,
    const std::vector<base::FilePath>& file_paths,
    const std::vector<std::string>& mime_types) {
  auto file_urls = apps::GetFileUrls(profile, file_paths);
  return CreateShareIntentFromFiles(file_urls, mime_types);
}

apps::mojom::IntentPtr CreateShareIntentFromFiles(
    Profile* profile,
    const std::vector<base::FilePath>& file_paths,
    const std::vector<std::string>& mime_types,
    const std::string& share_text,
    const std::string& share_title) {
  auto file_urls = apps::GetFileUrls(profile, file_paths);
  return CreateShareIntentFromFiles(file_urls, mime_types, share_text,
                                    share_title);
}

apps::mojom::IntentPtr CreateIntentForArcIntentAndActivity(
    arc::mojom::IntentInfoPtr arc_intent,
    arc::mojom::ActivityNamePtr activity) {
  auto intent = apps::mojom::Intent::New();
  if (arc_intent) {
    intent->action = std::move(arc_intent->action);
    intent->data = std::move(arc_intent->data);
    intent->mime_type = std::move(arc_intent->type);
    intent->categories = std::move(arc_intent->categories);
    intent->ui_bypassed = arc_intent->ui_bypassed
                              ? apps::mojom::OptionalBool::kTrue
                              : apps::mojom::OptionalBool::kFalse;
    intent->extras = std::move(arc_intent->extras);
  }

  if (activity) {
    intent->activity_name = std::move(activity->activity_name);
  }

  return intent;
}

base::flat_map<std::string, std::string> CreateArcIntentExtras(
    const apps::mojom::IntentPtr& intent) {
  auto extras = base::flat_map<std::string, std::string>();
  if (intent->share_text.has_value()) {
    // Slice off the "S." prefix for the key.
    extras.insert(std::make_pair(kIntentExtraText + kIntentPrefixLength,
                                 intent->share_text.value()));
  }
  if (intent->share_title.has_value()) {
    // Slice off the "S." prefix for the key.
    extras.insert(std::make_pair(kIntentExtraSubject + kIntentPrefixLength,
                                 intent->share_title.value()));
  }
  if (intent->start_type.has_value()) {
    // Slice off the "S." prefix for the key.
    extras.insert(std::make_pair(kIntentExtraStartType + kIntentPrefixLength,
                                 intent->start_type.value()));
  }
  return extras;
}

arc::mojom::IntentInfoPtr CreateArcIntent(
    const apps::mojom::IntentPtr& intent) {
  arc::mojom::IntentInfoPtr arc_intent;
  if (!intent->action.has_value() && !intent->url.has_value() &&
      !intent->share_text.has_value() && !intent->activity_name.has_value()) {
    return arc_intent;
  }

  arc_intent = arc::mojom::IntentInfo::New();
  if (intent->action.has_value()) {
    arc_intent->action = GetArcIntentAction(intent->action.value());
  } else {
    arc_intent->action = arc::kIntentActionView;
  }
  if (intent->url.has_value()) {
    arc_intent->data = intent->url->spec();
  }
  if (intent->share_text.has_value() || intent->share_title.has_value() ||
      intent->start_type.has_value()) {
    arc_intent->extras = CreateArcIntentExtras(intent);
  }
  if (intent->categories.has_value()) {
    arc_intent->categories = intent->categories;
  }
  if (intent->data.has_value()) {
    arc_intent->data = intent->data;
  }
  if (intent->mime_type.has_value()) {
    arc_intent->type = intent->mime_type;
  }
  if (intent->ui_bypassed != apps::mojom::OptionalBool::kUnknown) {
    arc_intent->ui_bypassed =
        intent->ui_bypassed == apps::mojom::OptionalBool::kTrue ? true : false;
  }
  if (intent->extras.has_value()) {
    arc_intent->extras = intent->extras;
  }
  return arc_intent;
}

std::string CreateLaunchIntent(const std::string& package_name,
                               const apps::mojom::IntentPtr& intent) {
  // If |intent| has |ui_bypassed|, |url| or |data|, it is too complex to
  // convert to a string, so return the empty string.
  if (intent->ui_bypassed != apps::mojom::OptionalBool::kUnknown ||
      intent->url.has_value() || intent->data.has_value()) {
    return std::string();
  }

  std::string ret = base::StringPrintf("%s;", arc::kIntentPrefix);

  // Convert action.
  std::string action;
  if (intent->action.has_value()) {
    action = GetArcIntentAction(intent->action.value());
  }
  ret += base::StringPrintf("%s=%s;", arc::kAction,
                            GetArcIntentAction(intent->action.value()));

  // Convert categories.
  if (intent->categories.has_value()) {
    for (const auto& category : intent->categories.value()) {
      ret += base::StringPrintf("%s=%s;", arc::kCategory, category.c_str());
    }
  }

  // Set launch flags.
  ret +=
      base::StringPrintf("%s=0x%x;", arc::kLaunchFlags,
                         arc::Intent::FLAG_ACTIVITY_NEW_TASK |
                             arc::Intent::FLAG_ACTIVITY_RESET_TASK_IF_NEEDED);

  // Convert activity_name.
  if (intent->activity_name.has_value()) {
    // Remove the |package_name| prefix, if activity starts with it.
    const std::string& activity = intent->activity_name.value();
    const char* activity_compact_name =
        activity.find(package_name.c_str()) == 0
            ? activity.c_str() + package_name.length()
            : activity.c_str();
    ret += base::StringPrintf("%s=%s/%s;", arc::kComponent,
                              package_name.c_str(), activity_compact_name);
  } else {
    ret += base::StringPrintf("%s=%s/;", arc::kComponent, package_name.c_str());
  }

  if (intent->mime_type.has_value()) {
    ret +=
        base::StringPrintf("%s=%s;", kType, intent->mime_type.value().c_str());
  }

  if (intent->share_text.has_value()) {
    ret += base::StringPrintf("%s=%s;", kIntentExtraText,
                              intent->share_text.value().c_str());
  }

  if (intent->share_title.has_value()) {
    ret += base::StringPrintf("%s=%s;", kIntentExtraSubject,
                              intent->share_title.value().c_str());
  }

  if (intent->start_type.has_value()) {
    ret += base::StringPrintf("%s=%s;", kIntentExtraStartType,
                              intent->start_type.value().c_str());
  }

  if (intent->extras.has_value()) {
    for (auto it : intent->extras.value()) {
      ret += base::StringPrintf("%s=%s;", it.first.c_str(), it.second.c_str());
    }
  }

  ret += arc::kEndSuffix;
  DCHECK(!ret.empty());
  return ret;
}

arc::IntentFilter CreateArcIntentFilter(
    const std::string& package_name,
    const apps::mojom::IntentFilterPtr& intent_filter) {
  std::vector<std::string> actions;
  std::vector<std::string> schemes;
  std::vector<arc::IntentFilter::AuthorityEntry> authorities;
  std::vector<arc::IntentFilter::PatternMatcher> paths;
  std::vector<std::string> mime_types;
  for (auto& condition : intent_filter->conditions) {
    switch (condition->condition_type) {
      case apps::mojom::ConditionType::kScheme:
        for (auto& condition_value : condition->condition_values) {
          schemes.push_back(condition_value->value);
        }
        break;
      case apps::mojom::ConditionType::kHost:
        for (auto& condition_value : condition->condition_values) {
          authorities.push_back(arc::IntentFilter::AuthorityEntry(
              /*host=*/condition_value->value, /*port=*/0));
        }
        break;
      case apps::mojom::ConditionType::kPattern:
        for (auto& condition_value : condition->condition_values) {
          arc::mojom::PatternType match_type;
          switch (condition_value->match_type) {
            case apps::mojom::PatternMatchType::kLiteral:
              match_type = arc::mojom::PatternType::PATTERN_LITERAL;
              break;
            case apps::mojom::PatternMatchType::kPrefix:
              match_type = arc::mojom::PatternType::PATTERN_PREFIX;
              break;
            case apps::mojom::PatternMatchType::kGlob:
              match_type = arc::mojom::PatternType::PATTERN_SIMPLE_GLOB;
              break;
            case apps::mojom::PatternMatchType::kNone:
            case apps::mojom::PatternMatchType::kMimeType:
              NOTREACHED();
              return arc::IntentFilter();
          }
          paths.push_back(arc::IntentFilter::PatternMatcher(
              condition_value->value, match_type));
        }
        break;
      case apps::mojom::ConditionType::kAction:
        for (auto& condition_value : condition->condition_values) {
          actions.push_back(GetArcIntentAction(condition_value->value));
        }
        break;
      case apps::mojom::ConditionType::kMimeType:
        for (auto& condition_value : condition->condition_values) {
          mime_types.push_back(condition_value->value);
        }
        break;
    }
  }
  // TODO(crbug.com/853604): Add support for other category types.
  return arc::IntentFilter(package_name, std::move(actions),
                           std::move(authorities), std::move(paths),
                           std::move(schemes), std::move(mime_types));
}

apps::mojom::IntentFilterPtr ConvertArcIntentFilter(
    const arc::IntentFilter& arc_intent_filter) {
  auto intent_filter = apps::mojom::IntentFilter::New();

  if (base::FeatureList::IsEnabled(features::kIntentHandlingSharing)) {
    std::vector<apps::mojom::ConditionValuePtr> action_condition_values;
    for (auto& arc_action : arc_intent_filter.actions()) {
      std::string action;
      if (arc_action == arc::kIntentActionView) {
        action = apps_util::kIntentActionView;
      } else if (arc_action == arc::kIntentActionSend) {
        action = apps_util::kIntentActionSend;
      } else if (arc_action == arc::kIntentActionSendMultiple) {
        action = apps_util::kIntentActionSendMultiple;
      } else {
        continue;
      }
      action_condition_values.push_back(apps_util::MakeConditionValue(
          action, apps::mojom::PatternMatchType::kNone));
    }
    if (!action_condition_values.empty()) {
      auto action_condition =
          apps_util::MakeCondition(apps::mojom::ConditionType::kAction,
                                   std::move(action_condition_values));
      intent_filter->conditions.push_back(std::move(action_condition));
    }
  }

  std::vector<apps::mojom::ConditionValuePtr> scheme_condition_values;
  for (auto& scheme : arc_intent_filter.schemes()) {
    scheme_condition_values.push_back(apps_util::MakeConditionValue(
        scheme, apps::mojom::PatternMatchType::kNone));
  }
  if (!scheme_condition_values.empty()) {
    auto scheme_condition =
        apps_util::MakeCondition(apps::mojom::ConditionType::kScheme,
                                 std::move(scheme_condition_values));
    intent_filter->conditions.push_back(std::move(scheme_condition));
  }

  std::vector<apps::mojom::ConditionValuePtr> host_condition_values;
  for (auto& authority : arc_intent_filter.authorities()) {
    host_condition_values.push_back(apps_util::MakeConditionValue(
        authority.host(), apps::mojom::PatternMatchType::kNone));
  }
  if (!host_condition_values.empty()) {
    auto host_condition = apps_util::MakeCondition(
        apps::mojom::ConditionType::kHost, std::move(host_condition_values));
    intent_filter->conditions.push_back(std::move(host_condition));
  }

  std::vector<apps::mojom::ConditionValuePtr> path_condition_values;
  for (auto& path : arc_intent_filter.paths()) {
    apps::mojom::PatternMatchType match_type;
    switch (path.match_type()) {
      case arc::mojom::PatternType::PATTERN_LITERAL:
        match_type = apps::mojom::PatternMatchType::kLiteral;
        break;
      case arc::mojom::PatternType::PATTERN_PREFIX:
        match_type = apps::mojom::PatternMatchType::kPrefix;
        break;
      case arc::mojom::PatternType::PATTERN_SIMPLE_GLOB:
        match_type = apps::mojom::PatternMatchType::kGlob;
        break;
    }
    path_condition_values.push_back(
        apps_util::MakeConditionValue(path.pattern(), match_type));
  }
  if (!path_condition_values.empty()) {
    auto path_condition = apps_util::MakeCondition(
        apps::mojom::ConditionType::kPattern, std::move(path_condition_values));
    intent_filter->conditions.push_back(std::move(path_condition));
  }

  if (base::FeatureList::IsEnabled(features::kIntentHandlingSharing)) {
    std::vector<apps::mojom::ConditionValuePtr> mime_type_condition_values;
    for (auto& mime_type : arc_intent_filter.mime_types()) {
      mime_type_condition_values.push_back(apps_util::MakeConditionValue(
          mime_type, apps::mojom::PatternMatchType::kMimeType));
    }
    if (!mime_type_condition_values.empty()) {
      auto mime_type_condition =
          apps_util::MakeCondition(apps::mojom::ConditionType::kMimeType,
                                   std::move(mime_type_condition_values));
      intent_filter->conditions.push_back(std::move(mime_type_condition));
    }
    if (!arc_intent_filter.activity_name().empty()) {
      intent_filter->activity_name = arc_intent_filter.activity_name();
    }
    if (!arc_intent_filter.activity_label().empty()) {
      intent_filter->activity_label = arc_intent_filter.activity_label();
    }
  }

  return intent_filter;
}
#endif

}  // namespace apps_util
