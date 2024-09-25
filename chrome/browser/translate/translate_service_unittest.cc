// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/translate/translate_service.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/url_constants.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/url_constants.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/file_manager/app_id.h"
#include "extensions/common/constants.h"
#endif

namespace translate {
namespace {

// Test the check that determines if a URL should be translated.
TEST(TranslateServiceTest, CheckTranslatableURL) {
  GURL empty_url = GURL(std::string());
  EXPECT_FALSE(TranslateService::IsTranslatableURL(empty_url));

  GURL about_blank_url = GURL("about:blank");
  EXPECT_FALSE(TranslateService::IsTranslatableURL(about_blank_url));

  std::string chrome = std::string(content::kChromeUIScheme) + "://flags";
  GURL chrome_url = GURL(chrome);
  EXPECT_FALSE(TranslateService::IsTranslatableURL(chrome_url));

  std::string devtools = std::string(content::kChromeDevToolsScheme) + "://";
  GURL devtools_url = GURL(devtools);
  EXPECT_FALSE(TranslateService::IsTranslatableURL(devtools_url));

  std::string chrome_native = std::string(chrome::kChromeNativeScheme) + "://";
  GURL chrome_native_url = GURL(chrome_native);
  EXPECT_FALSE(TranslateService::IsTranslatableURL(chrome_native_url));

  std::string file = std::string(url::kFileScheme) + "://";
  GURL file_url = GURL(file);
  EXPECT_TRUE(TranslateService::IsTranslatableURL(file_url));

  // kContentScheme is only used on Android.
#if BUILDFLAG(IS_ANDROID)
  std::string content = std::string(url::kContentScheme) + "://";
  GURL content_url = GURL(content);
  EXPECT_TRUE(TranslateService::IsTranslatableURL(content_url));
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::string filemanager = std::string(extensions::kExtensionScheme) +
                            std::string("://") +
                            std::string(file_manager::kFileManagerAppId);
  GURL filemanager_url = GURL(filemanager);
  EXPECT_FALSE(TranslateService::IsTranslatableURL(filemanager_url));
#endif

  GURL right_url = GURL("http://www.tamurayukari.com/");
  EXPECT_TRUE(TranslateService::IsTranslatableURL(right_url));
}

// Tests that download and history URLs are not translatable.
TEST(TranslateServiceTest, DownloadsAndHistoryNotTranslated) {
  content::BrowserTaskEnvironment task_environment;
  TranslateService::InitializeForTesting(
      network::mojom::ConnectionType::CONNECTION_WIFI);
  EXPECT_FALSE(
      TranslateService::IsTranslatableURL(GURL(chrome::kChromeUIDownloadsURL)));
  EXPECT_FALSE(
      TranslateService::IsTranslatableURL(GURL(chrome::kChromeUIHistoryURL)));
  TranslateService::ShutdownForTesting();
}

}  // namespace
}  // namespace translate
