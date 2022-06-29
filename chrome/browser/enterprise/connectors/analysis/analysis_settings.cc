// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/analysis_settings.h"

namespace enterprise_connectors {

CloudAnalysisSettings::CloudAnalysisSettings() = default;
CloudAnalysisSettings::CloudAnalysisSettings(CloudAnalysisSettings&&) = default;
CloudAnalysisSettings& CloudAnalysisSettings::operator=(
    CloudAnalysisSettings&&) = default;
CloudAnalysisSettings::CloudAnalysisSettings(const CloudAnalysisSettings&) =
    default;
CloudAnalysisSettings::~CloudAnalysisSettings() = default;

LocalAnalysisSettings::LocalAnalysisSettings() = default;
LocalAnalysisSettings::LocalAnalysisSettings(LocalAnalysisSettings&&) = default;
LocalAnalysisSettings& LocalAnalysisSettings::operator=(
    LocalAnalysisSettings&&) = default;
LocalAnalysisSettings::LocalAnalysisSettings(const LocalAnalysisSettings&) =
    default;
LocalAnalysisSettings::~LocalAnalysisSettings() = default;

CloudOrLocalAnalysisSettings::CloudOrLocalAnalysisSettings() = default;
CloudOrLocalAnalysisSettings::CloudOrLocalAnalysisSettings(
    CloudAnalysisSettings settings)
    : absl::variant<CloudAnalysisSettings, LocalAnalysisSettings>(
          std::move(settings)) {}
CloudOrLocalAnalysisSettings::CloudOrLocalAnalysisSettings(
    LocalAnalysisSettings settings)
    : absl::variant<CloudAnalysisSettings, LocalAnalysisSettings>(
          std::move(settings)) {}
CloudOrLocalAnalysisSettings::CloudOrLocalAnalysisSettings(
    CloudOrLocalAnalysisSettings&&) = default;
CloudOrLocalAnalysisSettings& CloudOrLocalAnalysisSettings::operator=(
    CloudOrLocalAnalysisSettings&&) = default;
CloudOrLocalAnalysisSettings::CloudOrLocalAnalysisSettings(
    const CloudOrLocalAnalysisSettings&) = default;
CloudOrLocalAnalysisSettings::~CloudOrLocalAnalysisSettings() = default;

const GURL& CloudOrLocalAnalysisSettings::analysis_url() const {
  DCHECK(absl::holds_alternative<CloudAnalysisSettings>(*this));
  return absl::get<CloudAnalysisSettings>(*this).analysis_url;
}

const std::string& CloudOrLocalAnalysisSettings::dm_token() const {
  DCHECK(absl::holds_alternative<CloudAnalysisSettings>(*this));
  return absl::get<CloudAnalysisSettings>(*this).dm_token;
}

const std::string CloudOrLocalAnalysisSettings::local_path() const {
  DCHECK(absl::holds_alternative<LocalAnalysisSettings>(*this));
  return absl::get<LocalAnalysisSettings>(*this).local_path;
}

AnalysisSettings::AnalysisSettings() = default;
AnalysisSettings::AnalysisSettings(AnalysisSettings&&) = default;
AnalysisSettings& AnalysisSettings::operator=(AnalysisSettings&&) = default;
AnalysisSettings::~AnalysisSettings() = default;

bool AnalysisSettings::is_cloud_analysis() const {
  return absl::holds_alternative<CloudAnalysisSettings>(
      cloud_or_local_settings);
}

bool AnalysisSettings::is_local_analysis() const {
  return absl::holds_alternative<LocalAnalysisSettings>(
      cloud_or_local_settings);
}

const CloudAnalysisSettings& AnalysisSettings::cloud_settings() const {
  DCHECK(is_cloud_analysis());
  return absl::get<CloudAnalysisSettings>(cloud_or_local_settings);
}

const LocalAnalysisSettings& AnalysisSettings::local_settings() const {
  DCHECK(is_local_analysis());
  return absl::get<LocalAnalysisSettings>(cloud_or_local_settings);
}

}  // namespace enterprise_connectors
