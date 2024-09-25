// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/arc_features_parser.h"

#include <string_view>

#include "ash/components/arc/arc_util.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/values.h"

namespace arc {

namespace {

constexpr const base::FilePath::CharType kArcVmFeaturesJsonFile[] =
    FILE_PATH_LITERAL("/etc/arcvm/features.json");
constexpr const base::FilePath::CharType kArcFeaturesJsonFile[] =
    FILE_PATH_LITERAL("/etc/arc/features.json");

base::RepeatingCallback<std::optional<ArcFeatures>()>*
    g_arc_features_getter_for_testing = nullptr;

std::optional<ArcFeatures> ParseFeaturesJson(std::string_view input_json) {
  ArcFeatures arc_features;

  auto parsed_json = base::JSONReader::ReadAndReturnValueWithError(input_json);
  if (!parsed_json.has_value()) {
    LOG(ERROR) << "Error parsing feature JSON: " << parsed_json.error().message;
    return std::nullopt;
  } else if (!parsed_json->is_dict()) {
    LOG(ERROR) << "Error parsing feature JSON: Expected a dictionary.";
    return std::nullopt;
  }

  const base::Value::Dict& dict = parsed_json->GetDict();

  // Parse each item under features.
  const base::Value::List* feature_list = dict.FindList("features");
  if (!feature_list) {
    LOG(ERROR) << "No feature list in JSON.";
    return std::nullopt;
  }
  for (auto& feature_item : *feature_list) {
    const std::string* feature_name = feature_item.GetDict().FindString("name");
    const std::optional<int> feature_version =
        feature_item.GetDict().FindInt("version");
    if (!feature_name || feature_name->empty()) {
      LOG(ERROR) << "Missing name in the feature.";
      return std::nullopt;
    }
    if (!feature_version.has_value()) {
      LOG(ERROR) << "Missing version in the feature.";
      return std::nullopt;
    }
    arc_features.feature_map.emplace(*feature_name, *feature_version);
  }

  // Parse each item under unavailable_features.
  const base::Value::List* unavailable_feature_list =
      dict.FindList("unavailable_features");
  if (!unavailable_feature_list) {
    LOG(ERROR) << "No unavailable feature list in JSON.";
    return std::nullopt;
  }
  for (auto& feature_item : *unavailable_feature_list) {
    if (!feature_item.is_string()) {
      LOG(ERROR) << "Item in the unavailable feature list is not a string.";
      return std::nullopt;
    }

    if (feature_item.GetString().empty()) {
      LOG(ERROR) << "Missing name in the feature.";
      return std::nullopt;
    }
    arc_features.unavailable_features.emplace_back(feature_item.GetString());
  }

  // Parse each item under build_props.
  const base::Value::Dict* properties = dict.FindDict("properties");
  if (!properties) {
    LOG(ERROR) << "No properties in JSON.";
    return std::nullopt;
  }

  constexpr char kFingerprintProperty[] = "ro.build.fingerprint";
  const std::string* fingerprint = properties->FindString(kFingerprintProperty);
  if (!fingerprint) {
    LOG(ERROR) << "Missing required build property " << kFingerprintProperty;
    return std::nullopt;
  }
  arc_features.build_props.fingerprint = *fingerprint;

  constexpr char kSdkProperty[] = "ro.build.version.sdk";
  const std::string* sdk_version = properties->FindString(kSdkProperty);
  if (!sdk_version) {
    LOG(ERROR) << "Missing required build property " << kSdkProperty;
    return std::nullopt;
  }
  arc_features.build_props.sdk_version = *sdk_version;

  constexpr char kReleaseProperty[] = "ro.build.version.release";
  const std::string* release_version = properties->FindString(kReleaseProperty);
  if (!release_version) {
    LOG(ERROR) << "Missing required build property " << kReleaseProperty;
    return std::nullopt;
  }
  arc_features.build_props.release_version = *release_version;

  constexpr char kAbiListProperty[] = "ro.product.cpu.abilist";
  constexpr char kSystemAbiListProperty[] = "ro.system.product.cpu.abilist";
  const std::string* abi_list = properties->FindString(kAbiListProperty);
  // On ARC T+, supported ABIs are listed in the "system" version of the
  // property.
  if (!abi_list) {
    abi_list = properties->FindString(kSystemAbiListProperty);
  }
  if (!abi_list) {
    LOG(ERROR) << "Missing required abilist build property";
    return std::nullopt;
  }
  arc_features.build_props.abi_list = *abi_list;

  // Parse the Play Store version
  const std::string* play_version = dict.FindString("play_store_version");
  if (!play_version) {
    LOG(ERROR) << "No Play Store version in JSON.";
    return std::nullopt;
  }
  arc_features.play_store_version = *play_version;

  return arc_features;
}

std::optional<ArcFeatures> ReadOnFileThread(const base::FilePath& file_path) {
  DCHECK(!file_path.empty());

  std::string input_json;
  {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);
    if (!base::ReadFileToString(file_path, &input_json)) {
      PLOG(ERROR) << "Cannot read file " << file_path.value()
                  << " into string.";
      return std::nullopt;
    }
  }

  if (input_json.empty()) {
    LOG(ERROR) << "Input JSON is empty in file " << file_path.value();
    return std::nullopt;
  }

  return ParseFeaturesJson(input_json);
}

}  // namespace

BuildPropsMapping::BuildPropsMapping() = default;
BuildPropsMapping::BuildPropsMapping(const BuildPropsMapping&) = default;
BuildPropsMapping& BuildPropsMapping::operator=(const BuildPropsMapping&) =
    default;
BuildPropsMapping::BuildPropsMapping(BuildPropsMapping&& other) = default;
BuildPropsMapping& BuildPropsMapping::operator=(BuildPropsMapping&& other) =
    default;
BuildPropsMapping::~BuildPropsMapping() = default;

ArcFeatures::ArcFeatures() = default;
ArcFeatures::ArcFeatures(ArcFeatures&& other) = default;
ArcFeatures::~ArcFeatures() = default;
ArcFeatures& ArcFeatures::operator=(ArcFeatures&& other) = default;

void ArcFeaturesParser::GetArcFeatures(
    base::OnceCallback<void(std::optional<ArcFeatures>)> callback) {
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

std::optional<ArcFeatures> ArcFeaturesParser::ParseFeaturesJsonForTesting(
    std::string_view input_json) {
  return ParseFeaturesJson(input_json);
}

void ArcFeaturesParser::SetArcFeaturesGetterForTesting(
    base::RepeatingCallback<std::optional<ArcFeatures>()>* getter) {
  g_arc_features_getter_for_testing = getter;
}

}  // namespace arc
