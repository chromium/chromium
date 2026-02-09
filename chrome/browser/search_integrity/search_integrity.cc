// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_integrity/search_integrity.h"

#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/common/chrome_paths.h"
#include "components/search_engines/template_url_service.h"

namespace search_integrity {

SearchIntegrity::SearchIntegrity(TemplateURLService* template_url_service,
                                 const base::FilePath& profile_path)
    : template_url_service_(template_url_service),
      profile_path_(profile_path) {}

SearchIntegrity::~SearchIntegrity() = default;

void SearchIntegrity::CheckSearchEngines() {
  // Asynchronously initialize the search engine allowlist on a background
  // thread to avoid blocking the UI thread.
  // Construct the path to the prepopulated_engines.json file, which
  // is a bundled resource.
  base::FilePath json_path;
  if (!base::PathService::Get(chrome::DIR_RESOURCES, &json_path)) {
    return;
  }
  json_path = json_path.Append(FILE_PATH_LITERAL("third_party"))
                  .Append(FILE_PATH_LITERAL("search_engines_data"))
                  .Append(FILE_PATH_LITERAL("resources"))
                  .Append(FILE_PATH_LITERAL("definitions"))
                  .Append(FILE_PATH_LITERAL("prepopulated_engines.json"));

  // Construct the path to the bloom filter file, which is stored in
  // the user's profile directory.
  base::FilePath bloom_filter_path =
      profile_path_.Append(FILE_PATH_LITERAL("engine_allowlist.bf"));

  // TODO(482017896):Initialize the singleton instance of the
  // SearchEngineAllowlist.

  // Run OnAllowlistInitialized on the original (UI) thread.
  OnAllowlistInitialized();
}

void SearchIntegrity::OnAllowlistInitialized() {
  if (!template_url_service_) {
    return;
  }

  auto template_urls = template_url_service_->GetTemplateURLs();
  const TemplateURL* default_search_provider =
      template_url_service_->GetDefaultSearchProvider();

  // Iterate through all installed search engines to check if any of them are
  // not in the allowlist.
  for (const TemplateURL* template_url : template_urls) {
    bool is_in_default_list = (template_url->prepopulate_id() > 0) ||
                              (template_url == default_search_provider) ||
                              (template_url->CreatedByPolicy());

    // Explicitly exclude Starter Pack engines (e.g. @history, @tabs, @gemini).
    if (template_url->starter_pack_id() !=
        template_url_starter_pack_data::StarterPackId::kNone) {
      is_in_default_list = false;
    }

    if (!is_in_default_list) {
      continue;
    }

    // TODO(482017896): iterate through the search engine list to find custom
    // option.
  }

  // TODO(482017896):Check if the default search provider is not in the
  // allowlist.

  // TODO(482041973): Add histogram to track count of custom options and track
  // count of default options.
}

}  // namespace search_integrity
