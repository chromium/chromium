// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/arc_features_parser.h"

#include <memory>

#include "ash/components/arc/arc_util.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/values.h"

namespace arc {

namespace {

constexpr const base::FilePath::CharType kArcVmFeaturesJsonFile[] =
    FILE_PATH_LITERAL("/etc/arcvm/features.json");
constexpr const base::FilePath::CharType kArcFeaturesJsonFile[] =
    FILE_PATH_LITERAL("/etc/arc/features.json");

base::RepeatingCallback<absl::optional<ArcFeatures>()>*
    g_arc_features_getter_for_testing = nullptr;

absl::optional<ArcFeatures> ParseFeaturesJson(base::StringPiece input_json) {
  ArcFeatures arc_features;

  auto parsed_json = base::JSONReader::ReadAndReturnValueWithError(input_json);
  if (!parsed_json.has_value()) {
    LOG(ERROR) << "Error parsing feature JSON: " << parsed_json.error().message;
    return absl::nullopt;
  } else if (!parsed_json->is_dict()) {
    LOG(ERROR) << "Error parsing feature JSON: Expected a dictionary.";
    return absl::nullopt;
  }

  // Parse each item under features.
  const base::Value* feature_list =
      parsed_json->FindKeyOfType("features", base::Value::Type::LIST);
  if (!feature_list) {
    LOG(ERROR) << "No feature list in JSON.";
    return absl::nullopt;
  }
  for (auto& feature_item : feature_list->GetListDeprecated()) {
    const base::Value* feature_name =
        feature_item.FindKeyOfType("name", base::Value::Type::STRING);
    const base::Value* feature_version =
        feature_item.FindKeyOfType("version", base::Value::Type::INTEGER);
    if (!feature_name || feature_name->GetString().empty()) {
      LOG(ERROR) << "Missing name in the feature.";
      return absl::nullopt;
    }
    if (!feature_version) {
      LOG(ERROR) << "Missing version in the feature.";
      return absl::nullopt;
    }
    arc_features.feature_map.emplace(feature_name->GetString(),
                                     feature_version->GetInt());
  }

  // Parse each item under unavailable_features.
  const base::Value* unavailable_feature_list = parsed_json->FindKeyOfType(
      "unavailable_features", base::Value::Type::LIST);
  if (!unavailable_feature_list) {
    LOG(ERROR) << "No unavailable feature list in JSON.";
    return absl::nullopt;
  }
  for (auto& feature_item : unavailable_feature_list->GetListDeprecated()) {
    if (!feature_item.is_string()) {
      LOG(ERROR) << "Item in the unavailable feature list is not a string.";
      return absl::nullopt;
    }

    if (feature_item.GetString().empty()) {
      LOG(ERROR) << "Missing name in the feature.";
      return absl::nullopt;
    }
    arc_features.unavailable_features.emplace_back(feature_item.GetString());
  }

  // Parse each item under properties.
  const base::Value* properties =
      parsed_json->FindKeyOfType("properties", base::Value::Type::DICTIONARY);
  if (!properties) {
    LOG(ERROR) << "No properties in JSON.";
    return absl::nullopt;
  }
  for (const auto item : properties->DictItems()) {
    if (!item.second.is_string()) {
      LOG(ERROR) << "Item in the properties mapping is not a string.";
      return absl::nullopt;
    }

    arc_features.build_props.emplace(item.first, item.second.GetString());
  }

  // Parse the Play Store version
  const base::Value* play_version = parsed_json->FindKeyOfType(
      "play_store_version", base::Value::Type::STRING);
  if (!play_version) {
    LOG(ERROR) << "No Play Store version in JSON.";
    return absl::nullopt;
  }
  arc_features.play_store_version = play_version->GetString();

  return arc_features;
}

absl::optional<ArcFeatures> ReadOnFileThread(const base::FilePath& file_path) {
  DCHECK(!file_path.empty());
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  std::string input_json;
  {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);
    if (!base::ReadFileToString(file_path, &input_json)) {
      PLOG(ERROR) << "Cannot read file " << file_path.value()
                  << " into string.";
      return absl::nullopt;
    }
  }

  if (input_json.empty()) {
    LOG(ERROR) << "Input JSON is empty in file " << file_path.value();
    return absl::nullopt;
  }

  return ParseFeaturesJson(input_json);
}

}  // namespace

ArcFeatures::ArcFeatures() = default;
ArcFeatures::ArcFeatures(ArcFeatures&& other) = default;
ArcFeatures::~ArcFeatures() = default;
ArcFeatures& ArcFeatures::operator=(ArcFeatures&& other) = default;

void ArcFeaturesParser::GetArcFeatures(
    base::OnceCallback<void(absl::optional<ArcFeatures>)> callback) {
  if (g_arc_features_getter_for_testing) {
    std::move(callback).Run(g_arc_features_getter_for_testing->Run());
    return;
  }

  const auto* json_file =
      arc::IsArcVmEnabled() ? kArcVmFeaturesJsonFile : kArcFeaturesJsonFile;
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&ReadOnFileThread, base::FilePath(json_file)),
      std::move(callback));
}

absl::optional<ArcFeatures> ArcFeaturesParser::ParseFeaturesJsonForTesting(
    base::StringPiece input_json) {
  return ParseFeaturesJson(input_json);
}

void ArcFeaturesParser::SetArcFeaturesGetterForTesting(
    base::RepeatingCallback<absl::optional<ArcFeatures>()>* getter) {
  g_arc_features_getter_for_testing = getter;
}

}  // namespace arc
