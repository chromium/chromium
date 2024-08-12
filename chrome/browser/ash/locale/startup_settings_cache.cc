// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/locale/startup_settings_cache.h"

#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/path_service.h"
#include "base/values.h"
#include "chrome/common/chrome_paths.h"

namespace ash::startup_settings_cache {
namespace {

// Name of the cache file on disk.
const char kCacheFilename[] = "startup_settings_cache.json";

// JSON dictionary key for application locale (e.g. "ja" or "en_GB").
const char kAppLocaleKey[] = "app_locale";

bool GetCacheFilePath(base::FilePath* path) {
  base::FilePath user_data_dir;
  if (!base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir)) {
    return false;
  }

  *path = user_data_dir.Append(kCacheFilename);
  return true;
}

}  // namespace

std::string ReadAppLocale() {
  base::FilePath cache_file;
  if (!GetCacheFilePath(&cache_file)) {
    return std::string();
  }

  std::string input;
  if (!base::ReadFileToString(cache_file, &input)) {
    return std::string();
  }

  std::optional<base::Value> settings = base::JSONReader::Read(input);
  if (!settings.has_value()) {
    return std::string();
  }

  const std::string* app_locale_setting =
      settings->GetDict().FindString(kAppLocaleKey);
  // The locale is already an "actual locale", so this does not need to call
  // language::ConvertToActualUILocale().
  return app_locale_setting ? *app_locale_setting : std::string();
}

void WriteAppLocale(const std::string& app_locale) {
  base::FilePath cache_file;
  if (!GetCacheFilePath(&cache_file)) {
    return;
  }

  auto settings = base::Value::Dict().Set(kAppLocaleKey, app_locale);
  std::string output;
  if (!base::JSONWriter::Write(settings, &output)) {
    return;
  }

  // Ignore errors because we're shutting down and we can't recover.
  base::WriteFile(cache_file, output);
}

}  // namespace ash::startup_settings_cache
