// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/external_testing_loader.h"

#include "base/json/json_string_value_serializer.h"
#include "base/values.h"
#include "chrome/browser/extensions/external_pref_loader.h"
#include "content/public/browser/browser_thread.h"

namespace extensions {

ExternalTestingLoader::ExternalTestingLoader(
    const std::string& json_data,
    const base::FilePath& fake_base_path)
    : fake_base_path_(fake_base_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  JSONStringValueDeserializer deserializer(json_data);
  base::FilePath fake_json_path = fake_base_path.AppendASCII("fake.json");
  testing_prefs_ =
      ExternalPrefLoader::ExtractExtensionPrefs(&deserializer, fake_json_path);
}

const base::FilePath ExternalTestingLoader::GetBaseCrxFilePath() {
  return fake_base_path_;
}

void ExternalTestingLoader::StartLoading() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  LoadFinished(testing_prefs_.Clone());
}

ExternalTestingLoader::~ExternalTestingLoader() = default;

}  // namespace extensions
