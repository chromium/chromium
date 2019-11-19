// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/settings_reset_prompt/default_settings_fetcher.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/task/post_task.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/google/google_brand.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profile_resetter/brandcode_config_fetcher.h"
#include "chrome/browser/profile_resetter/brandcoded_default_settings.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "url/gurl.h"

namespace safe_browsing {

namespace {

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
constexpr char kOmahaUrl[] = "https://tools.google.com/service/update2";
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

}  // namespace

// static
void DefaultSettingsFetcher::FetchDefaultSettings(SettingsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // |settings_fetcher| will delete itself when default settings have been
  // fetched after the call to |Start()|.
  DefaultSettingsFetcher* settings_fetcher =
      new DefaultSettingsFetcher(std::move(callback));
  settings_fetcher->Start();
}

// static
void DefaultSettingsFetcher::FetchDefaultSettingsForTesting(
    SettingsCallback callback,
    std::unique_ptr<BrandcodedDefaultSettings> fetched_settings) {
  DefaultSettingsFetcher* settings_fetcher =
      new DefaultSettingsFetcher(std::move(callback));
  // Inject the given settings directly by passing them to
  // |PostCallbackAndDeleteSelf()|.
  settings_fetcher->PostCallbackAndDeleteSelf(std::move(fetched_settings));
}

DefaultSettingsFetcher::DefaultSettingsFetcher(SettingsCallback callback)
    : callback_(std::move(callback)) {}

DefaultSettingsFetcher::~DefaultSettingsFetcher() {}

void DefaultSettingsFetcher::Start() {
  DCHECK(!config_fetcher_);

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  std::string brandcode;
  if (google_brand::GetBrand(&brandcode) && !brandcode.empty()) {
    config_fetcher_.reset(new BrandcodeConfigFetcher(
        g_browser_process->system_network_context_manager()
            ->GetURLLoaderFactory(),
        base::Bind(&DefaultSettingsFetcher::OnSettingsFetched,
                   base::Unretained(this)),
        GURL(kOmahaUrl), brandcode));
    return;
  }
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

  // For non Google Chrome builds and cases with an empty |brandcode|, we create
  // a default-constructed |BrandcodedDefaultSettings| object and post the
  // callback immediately.
  PostCallbackAndDeleteSelf(std::make_unique<BrandcodedDefaultSettings>());
}

void DefaultSettingsFetcher::OnSettingsFetched() {
  DCHECK(config_fetcher_);
  DCHECK(!config_fetcher_->IsActive());

  PostCallbackAndDeleteSelf(config_fetcher_->GetSettings());
}

void DefaultSettingsFetcher::PostCallbackAndDeleteSelf(
    std::unique_ptr<BrandcodedDefaultSettings> default_settings) {
  // Use default settings if fetching of BrandcodedDefaultSettings failed.
  if (!default_settings)
    default_settings.reset(new BrandcodedDefaultSettings());

  base::PostTask(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(std::move(callback_), std::move(default_settings)));
  delete this;
}

}  // namespace safe_browsing
