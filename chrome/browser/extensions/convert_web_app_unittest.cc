// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/convert_web_app.h"

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/version.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/extensions/manifest_handlers/linked_app_icons.h"
#include "chrome/common/web_application_info.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "components/services/app_service/public/cpp/file_handler_info.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_icon_set.h"
#include "extensions/common/extension_resource.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/file_handler_info.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/common/manifest_handlers/web_app_file_handler.h"
#include "extensions/common/manifest_handlers/web_app_linked_shortcut_items.h"
#include "extensions/common/manifest_handlers/web_app_shortcut_icons_handler.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/url_pattern.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/codec/png_codec.h"
#include "url/gurl.h"

namespace extensions {

namespace keys = manifest_keys;

namespace {

// Returns an icon bitmap corresponding to a canned icon size.
SkBitmap GetIconBitmap(int size) {
  SkBitmap result;

  base::FilePath icon_file;
  if (!base::PathService::Get(chrome::DIR_TEST_DATA, &icon_file)) {
    ADD_FAILURE() << "Could not get test data directory.";
    return result;
  }

  icon_file = icon_file.AppendASCII("extensions")
                       .AppendASCII("convert_web_app")
                       .AppendASCII(base::StringPrintf("%i.png", size));

  std::string icon_data;
  if (!base::ReadFileToString(icon_file, &icon_data)) {
    ADD_FAILURE() << "Could not read test icon.";
    return result;
  }

  if (!gfx::PNGCodec::Decode(
          reinterpret_cast<const unsigned char*>(icon_data.c_str()),
          icon_data.size(), &result)) {
    ADD_FAILURE() << "Could not decode test icon.";
    return result;
  }

  return result;
}

base::Time GetTestTime(int year, int month, int day, int hour, int minute,
                       int second, int millisecond) {
  base::Time::Exploded exploded = {0};
  exploded.year = year;
  exploded.month = month;
  exploded.day_of_month = day;
  exploded.hour = hour;
  exploded.minute = minute;
  exploded.second = second;
  exploded.millisecond = millisecond;
  base::Time out_time;
  EXPECT_TRUE(base::Time::FromUTCExploded(exploded, &out_time));
  return out_time;
}

}  // namespace

class ExtensionFromWebApp : public extensions::ExtensionServiceTestBase {
 public:
  void SetUp() override {
    extensions::ExtensionServiceTestBase::SetUp();
    ASSERT_TRUE(extensions_dir_.CreateUniqueTempDir());
  }

  void StartExtensionService() {
    InitializeEmptyExtensionService();
    service()->Init();
    base::RunLoop().RunUntilIdle();
    ASSERT_TRUE(ExtensionSystem::Get(service()->profile())->is_ready());
  }

  const base::FilePath& ExtensionPath() const {
    return extensions_dir_.GetPath();
  }

 private:
  base::ScopedTempDir extensions_dir_;
};

class ExtensionFromWebAppWithShortcutsMenu : public ExtensionFromWebApp {
 public:
  ExtensionFromWebAppWithShortcutsMenu() {
    scoped_feature_list.InitAndEnableFeature(
        features::kDesktopPWAsAppIconShortcutsMenu);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list;
};

TEST_F(ExtensionFromWebApp, GetScopeURLFromBookmarkApp) {
  StartExtensionService();
  base::DictionaryValue manifest;
  manifest.SetString(keys::kName, "Test App");
  manifest.SetString(keys::kVersion, "0");
  manifest.SetString(keys::kLaunchWebURL, "http://aaronboodman.com/gearpad/");

  // Create a "url_handlers" dictionary with one URL handler generated from
  // the scope.
  // {
  //   "scope": {
  //     "matches": [ "http://aaronboodman.com/gearpad/*" ],
  //     "title": "Test App"
  //   },
  // }
  GURL scope_url = GURL("http://aaronboodman.com/gearpad/");
  manifest.SetDictionary(keys::kUrlHandlers,
                         CreateURLHandlersForBookmarkApp(
                             scope_url, base::ASCIIToUTF16("Test App")));

  std::string error;
  scoped_refptr<Extension> bookmark_app =
      Extension::Create(ExtensionPath(), Manifest::INTERNAL, manifest,
                        Extension::FROM_BOOKMARK, &error);
  ASSERT_TRUE(bookmark_app.get());

  EXPECT_EQ(scope_url, GetScopeURLFromBookmarkApp(bookmark_app.get()));
}

TEST_F(ExtensionFromWebApp, GetScopeURLFromBookmarkApp_NoURLHandlers) {
  StartExtensionService();
  base::DictionaryValue manifest;
  manifest.SetString(keys::kName, "Test App");
  manifest.SetString(keys::kVersion, "0");
  manifest.SetString(keys::kLaunchWebURL, "http://aaronboodman.com/gearpad/");
  manifest.SetDictionary(keys::kUrlHandlers,
                         std::make_unique<base::DictionaryValue>());

  std::string error;
  scoped_refptr<Extension> bookmark_app =
      Extension::Create(ExtensionPath(), Manifest::INTERNAL, manifest,
                        Extension::FROM_BOOKMARK, &error);
  ASSERT_TRUE(bookmark_app.get());

  EXPECT_EQ(GURL(), GetScopeURLFromBookmarkApp(bookmark_app.get()));
}

TEST_F(ExtensionFromWebApp, GetScopeURLFromBookmarkApp_WrongURLHandler) {
  StartExtensionService();
  base::DictionaryValue manifest;
  manifest.SetString(keys::kName, "Test App");
  manifest.SetString(keys::kVersion, "0");
  manifest.SetString(keys::kLaunchWebURL, "http://aaronboodman.com/gearpad/");

  // Create a "url_handlers" dictionary with one URL handler not generated
  // from the scope.
  // {
  //   "test_url_handler": {
  //     "matches": [ "http://*.aaronboodman.com/" ],
  //     "title": "test handler"
  //   }
  // }
  auto test_matches = std::make_unique<base::ListValue>();
  test_matches->AppendString("http://*.aaronboodman.com/");

  auto test_handler = std::make_unique<base::DictionaryValue>();
  test_handler->SetList(keys::kMatches, std::move(test_matches));
  test_handler->SetString(keys::kUrlHandlerTitle, "test handler");

  auto url_handlers = std::make_unique<base::DictionaryValue>();
  url_handlers->SetDictionary("test_url_handler", std::move(test_handler));
  manifest.SetDictionary(keys::kUrlHandlers, std::move(url_handlers));

  std::string error;
  scoped_refptr<Extension> bookmark_app =
      Extension::Create(ExtensionPath(), Manifest::INTERNAL, manifest,
                        Extension::FROM_BOOKMARK, &error);
  ASSERT_TRUE(bookmark_app.get());

  EXPECT_EQ(GURL(), GetScopeURLFromBookmarkApp(bookmark_app.get()));
}

TEST_F(ExtensionFromWebApp, GetScopeURLFromBookmarkApp_ExtraURLHandler) {
  StartExtensionService();
  base::DictionaryValue manifest;
  manifest.SetString(keys::kName, "Test App");
  manifest.SetString(keys::kVersion, "0");
  manifest.SetString(keys::kLaunchWebURL, "http://aaronboodman.com/gearpad/");

  // Create a "url_handlers" dictionary with two URL handlers. One for
  // the scope and and extra one for testing.
  // {
  //   "scope": {
  //     "matches": [ "http://aaronboodman.com/gearpad/*" ],
  //     "title": "Test App"
  //   },
  //   "test_url_handler": {
  //     "matches": [ "http://*.aaronboodman.com/" ],
  //     "title": "test handler"
  //   }
  // }
  GURL scope_url = GURL("http://aaronboodman.com/gearpad/");
  std::unique_ptr<base::DictionaryValue> url_handlers =
      CreateURLHandlersForBookmarkApp(scope_url,
                                      base::ASCIIToUTF16("Test App"));

  auto test_matches = std::make_unique<base::ListValue>();
  test_matches->AppendString("http://*.aaronboodman.com/");

  auto test_handler = std::make_unique<base::DictionaryValue>();
  test_handler->SetList(keys::kMatches, std::move(test_matches));
  test_handler->SetString(keys::kUrlHandlerTitle, "test handler");

  url_handlers->SetDictionary("test_url_handler", std::move(test_handler));
  manifest.SetDictionary(keys::kUrlHandlers, std::move(url_handlers));

  std::string error;
  scoped_refptr<Extension> bookmark_app =
      Extension::Create(ExtensionPath(), Manifest::INTERNAL, manifest,
                        Extension::FROM_BOOKMARK, &error);
  ASSERT_TRUE(bookmark_app.get());

  // Check that we can retrieve the scope even if there is an extra
  // url handler.
  EXPECT_EQ(scope_url, GetScopeURLFromBookmarkApp(bookmark_app.get()));
}

TEST_F(ExtensionFromWebApp, GenerateVersion) {
  StartExtensionService();
  EXPECT_EQ("2010.1.1.0",
            ConvertTimeToExtensionVersion(
                GetTestTime(2010, 1, 1, 0, 0, 0, 0)));
  EXPECT_EQ("2010.12.31.22111",
            ConvertTimeToExtensionVersion(
                GetTestTime(2010, 12, 31, 8, 5, 50, 500)));
  EXPECT_EQ("2010.10.1.65535",
            ConvertTimeToExtensionVersion(
                GetTestTime(2010, 10, 1, 23, 59, 59, 999)));
}

TEST_F(ExtensionFromWebApp, Basic) {
  StartExtensionService();
  WebApplicationInfo web_app;
  web_app.title = base::ASCIIToUTF16("Gearpad");
  web_app.description =
      base::ASCIIToUTF16("The best text editor in the universe!");
  web_app.start_url = GURL("http://aaronboodman.com/gearpad/");
  web_app.scope = GURL("http://aaronboodman.com/gearpad/");

  const int sizes[] = {16, 48, 128};
  for (size_t i = 0; i < base::size(sizes); ++i) {
    WebApplicationIconInfo icon_info;
    icon_info.url =
        web_app.start_url.Resolve(base::StringPrintf("%i.png", sizes[i]));
    icon_info.square_size_px = sizes[i];
    web_app.icon_infos.push_back(std::move(icon_info));
    web_app.icon_bitmaps_any[sizes[i]] = GetIconBitmap(sizes[i]);
  }

  scoped_refptr<Extension> extension = ConvertWebAppToExtension(
      web_app, GetTestTime(1978, 12, 11, 0, 0, 0, 0), ExtensionPath(),
      Extension::NO_FLAGS, Manifest::INTERNAL);
  ASSERT_TRUE(extension.get());

  base::ScopedTempDir extension_dir;
  EXPECT_TRUE(extension_dir.Set(extension->path()));

  EXPECT_TRUE(extension->is_app());
  EXPECT_TRUE(extension->is_hosted_app());
  EXPECT_TRUE(extension->from_bookmark());
  EXPECT_FALSE(extension->is_legacy_packaged_app());

  EXPECT_FALSE(extension->was_installed_by_default());
  EXPECT_FALSE(extension->was_installed_by_oem());
  EXPECT_FALSE(extension->from_webstore());
  EXPECT_EQ(Manifest::INTERNAL, extension->location());

  EXPECT_EQ("zVvdNZy3Mp7CFU8JVSyXNlDuHdVLbP7fDO3TGVzj/0w=",
            extension->public_key());
  EXPECT_EQ("oplhagaaipaimkjlbekcdjkffijdockj", extension->id());
  EXPECT_EQ("1978.12.11.0", extension->version().GetString());
  EXPECT_EQ(base::UTF16ToUTF8(web_app.title), extension->name());
  EXPECT_EQ(base::UTF16ToUTF8(web_app.description), extension->description());
  EXPECT_EQ(web_app.start_url,
            AppLaunchInfo::GetFullLaunchURL(extension.get()));
  EXPECT_EQ(web_app.scope, GetScopeURLFromBookmarkApp(extension.get()));
  EXPECT_EQ(0u,
            extension->permissions_data()->active_permissions().apis().size());
  ASSERT_EQ(0u, extension->web_extent().patterns().size());

  const LinkedAppIcons& linked_icons =
      LinkedAppIcons::GetLinkedAppIcons(extension.get());
  EXPECT_EQ(web_app.icon_infos.size(), linked_icons.icons.size());
  for (size_t i = 0; i < web_app.icon_infos.size(); ++i) {
    EXPECT_EQ(web_app.icon_infos[i].url, linked_icons.icons[i].url);
    EXPECT_EQ(web_app.icon_infos[i].square_size_px, linked_icons.icons[i].size);
  }

  EXPECT_EQ(web_app.icon_bitmaps_any.size(),
            IconsInfo::GetIcons(extension.get()).map().size());
  for (const std::pair<const SquareSizePx, SkBitmap>& icon :
       web_app.icon_bitmaps_any) {
    int size = icon.first;
    EXPECT_EQ(base::StringPrintf("icons/%i.png", size),
              IconsInfo::GetIcons(extension.get())
                  .Get(size, ExtensionIconSet::MATCH_EXACTLY));
    ExtensionResource resource = IconsInfo::GetIconResource(
        extension.get(), size, ExtensionIconSet::MATCH_EXACTLY);
    ASSERT_TRUE(!resource.empty());
    EXPECT_TRUE(base::PathExists(resource.GetFilePath()));
  }
}

TEST_F(ExtensionFromWebApp, Minimal) {
  StartExtensionService();
  WebApplicationInfo web_app;
  web_app.title = base::ASCIIToUTF16("Gearpad");
  web_app.start_url = GURL("http://aaronboodman.com/gearpad/");

  scoped_refptr<Extension> extension = ConvertWebAppToExtension(
      web_app, GetTestTime(1978, 12, 11, 0, 0, 0, 0), ExtensionPath(),
      Extension::NO_FLAGS, Manifest::INTERNAL);
  ASSERT_TRUE(extension.get());

  base::ScopedTempDir extension_dir;
  EXPECT_TRUE(extension_dir.Set(extension->path()));

  EXPECT_TRUE(extension->is_app());
  EXPECT_TRUE(extension->is_hosted_app());
  EXPECT_TRUE(extension->from_bookmark());
  EXPECT_FALSE(extension->is_legacy_packaged_app());

  EXPECT_FALSE(extension->was_installed_by_default());
  EXPECT_FALSE(extension->was_installed_by_oem());
  EXPECT_FALSE(extension->from_webstore());
  EXPECT_EQ(Manifest::INTERNAL, extension->location());

  EXPECT_EQ("zVvdNZy3Mp7CFU8JVSyXNlDuHdVLbP7fDO3TGVzj/0w=",
            extension->public_key());
  EXPECT_EQ("oplhagaaipaimkjlbekcdjkffijdockj", extension->id());
  EXPECT_EQ("1978.12.11.0", extension->version().GetString());
  EXPECT_EQ(base::UTF16ToUTF8(web_app.title), extension->name());
  EXPECT_EQ("", extension->description());
  EXPECT_EQ(web_app.start_url,
            AppLaunchInfo::GetFullLaunchURL(extension.get()));
  EXPECT_TRUE(GetScopeURLFromBookmarkApp(extension.get()).is_empty());
  EXPECT_EQ(0u, IconsInfo::GetIcons(extension.get()).map().size());
  EXPECT_EQ(0u,
            extension->permissions_data()->active_permissions().apis().size());
  ASSERT_EQ(0u, extension->web_extent().patterns().size());
}

TEST_F(ExtensionFromWebApp, ExtraInstallationFlags) {
  StartExtensionService();
  WebApplicationInfo web_app;
  web_app.title = base::ASCIIToUTF16("Gearpad");
  web_app.start_url = GURL("http://aaronboodman.com/gearpad/");

  scoped_refptr<Extension> extension = ConvertWebAppToExtension(
      web_app, GetTestTime(1978, 12, 11, 0, 0, 0, 0), ExtensionPath(),
      Extension::FROM_WEBSTORE | Extension::WAS_INSTALLED_BY_OEM,
      Manifest::INTERNAL);
  ASSERT_TRUE(extension.get());

  EXPECT_TRUE(extension->is_app());
  EXPECT_TRUE(extension->is_hosted_app());
  EXPECT_TRUE(extension->from_bookmark());
  EXPECT_FALSE(extension->is_legacy_packaged_app());

  EXPECT_TRUE(extension->was_installed_by_oem());
  EXPECT_TRUE(extension->from_webstore());
  EXPECT_FALSE(extension->was_installed_by_default());
  EXPECT_EQ(Manifest::INTERNAL, extension->location());
}

TEST_F(ExtensionFromWebApp, ExternalPolicyLocation) {
  StartExtensionService();
  WebApplicationInfo web_app;
  web_app.title = base::ASCIIToUTF16("Gearpad");
  web_app.start_url = GURL("http://aaronboodman.com/gearpad/");

  scoped_refptr<Extension> extension = ConvertWebAppToExtension(
      web_app, GetTestTime(1978, 12, 11, 0, 0, 0, 0), ExtensionPath(),
      Extension::NO_FLAGS, Manifest::EXTERNAL_POLICY);
  ASSERT_TRUE(extension.get());

  EXPECT_TRUE(extension->is_app());
  EXPECT_TRUE(extension->is_hosted_app());
  EXPECT_TRUE(extension->from_bookmark());
  EXPECT_FALSE(extension->is_legacy_packaged_app());

  EXPECT_EQ(Manifest::EXTERNAL_POLICY, extension->location());
}

// Tests that a scope not ending in "/" works correctly.
// The tested behavior is unexpected but is working correctly according
// to the Web Manifest spec. https://github.com/w3c/manifest/issues/554
TEST_F(ExtensionFromWebApp, ScopeDoesNotEndInSlash) {
  StartExtensionService();
  WebApplicationInfo web_app;
  web_app.title = base::ASCIIToUTF16("Gearpad");
  web_app.description =
      base::ASCIIToUTF16("The best text editor in the universe!");
  web_app.start_url = GURL("http://aaronboodman.com/gearpad/");
  web_app.scope = GURL("http://aaronboodman.com/gear");

  scoped_refptr<Extension> extension = ConvertWebAppToExtension(
      web_app, GetTestTime(1978, 12, 11, 0, 0, 0, 0), ExtensionPath(),
      Extension::NO_FLAGS, Manifest::INTERNAL);
  ASSERT_TRUE(extension.get());
  EXPECT_EQ(web_app.scope, GetScopeURLFromBookmarkApp(extension.get()));
}

// Tests that |file_handler| on the WebAppManifest is correctly converted
// to |file_handlers| on an extension manifest.
TEST_F(ExtensionFromWebApp, FileHandlersAreCorrectlyConverted) {
  StartExtensionService();
  WebApplicationInfo web_app;
  web_app.title = base::ASCIIToUTF16("Graphr");
  web_app.description = base::ASCIIToUTF16("A magical graphy thing");
  web_app.start_url = GURL("https://graphr.n/");
  web_app.scope = GURL("https://graphr.n/");

  {
    blink::Manifest::FileHandler graph;
    graph.action = GURL("https://graphr.n/open-graph/");
    graph.name = base::ASCIIToUTF16("Graph");
    graph.accept[base::ASCIIToUTF16("text/svg+xml")].push_back(
        base::ASCIIToUTF16(""));
    graph.accept[base::ASCIIToUTF16("text/svg+xml")].push_back(
        base::ASCIIToUTF16(".svg"));
    web_app.file_handlers.push_back(graph);

    blink::Manifest::FileHandler raw;
    raw.action = GURL("https://graphr.n/open-raw/");
    raw.name = base::ASCIIToUTF16("Raw");
    raw.accept[base::ASCIIToUTF16("text/csv")].push_back(
        base::ASCIIToUTF16(".csv"));
    web_app.file_handlers.push_back(raw);
  }

  scoped_refptr<Extension> extension = ConvertWebAppToExtension(
      web_app, GetTestTime(1978, 12, 11, 0, 0, 0, 0), ExtensionPath(),
      Extension::NO_FLAGS, Manifest::INTERNAL);

  ASSERT_TRUE(extension.get());

  const std::vector<apps::FileHandlerInfo>* file_handler_infos =
      extensions::FileHandlers::GetFileHandlers(extension.get());

  ASSERT_TRUE(file_handler_infos);
  EXPECT_EQ(2u, file_handler_infos->size());

  {
    const apps::FileHandlerInfo& info = file_handler_infos->at(0);
    EXPECT_EQ("https://graphr.n/open-graph/", info.id);
    EXPECT_FALSE(info.include_directories);
    EXPECT_EQ(apps::file_handler_verbs::kOpenWith, info.verb);
    // Extensions should contain SVG, and only SVG
    EXPECT_THAT(info.extensions, testing::UnorderedElementsAre("svg"));
    // Mime types should contain text/svg+xml and only text/svg+xml
    EXPECT_THAT(info.types, testing::UnorderedElementsAre("text/svg+xml"));
  }
  {
    const apps::FileHandlerInfo& info = file_handler_infos->at(1);
    EXPECT_EQ("https://graphr.n/open-raw/", info.id);
    EXPECT_FALSE(info.include_directories);
    EXPECT_EQ(apps::file_handler_verbs::kOpenWith, info.verb);
    // Extensions should contain csv, and only csv
    EXPECT_THAT(info.extensions, testing::UnorderedElementsAre("csv"));
    // Mime types should contain text/csv and only text/csv
    EXPECT_THAT(info.types, testing::UnorderedElementsAre("text/csv"));
  }
}

// Tests that |file_handler| on the WebAppManifest is correctly converted
// to |web_app_file_handlers| on an extension manifest.
TEST_F(ExtensionFromWebApp, WebAppFileHandlersAreCorrectlyConverted) {
  StartExtensionService();
  WebApplicationInfo web_app;
  web_app.title = base::ASCIIToUTF16("Graphr");
  web_app.description = base::ASCIIToUTF16("A magical graphy thing.");
  web_app.start_url = GURL("https://graphr.n/");
  web_app.scope = GURL("https://graphr.n");

  {
    blink::Manifest::FileHandler file_handler;
    file_handler.action = GURL("https://graphr.n/open-graph/");
    file_handler.name = base::ASCIIToUTF16("Graph");
    file_handler.accept[base::ASCIIToUTF16("text/svg+xml")].push_back(
        base::ASCIIToUTF16(""));
    file_handler.accept[base::ASCIIToUTF16("text/svg+xml")].push_back(
        base::ASCIIToUTF16(".svg"));
    web_app.file_handlers.push_back(file_handler);
  }
  {
    blink::Manifest::FileHandler file_handler;
    file_handler.action = GURL("https://graphr.n/open-raw/");
    file_handler.name = base::ASCIIToUTF16("Raw");
    file_handler.accept[base::ASCIIToUTF16("text/csv")].push_back(
        base::ASCIIToUTF16(".csv"));
    web_app.file_handlers.push_back(file_handler);
  }

  scoped_refptr<Extension> extension = ConvertWebAppToExtension(
      web_app, GetTestTime(1978, 12, 11, 0, 0, 0, 0), ExtensionPath(),
      Extension::NO_FLAGS, Manifest::INTERNAL);

  ASSERT_TRUE(extension.get());

  const apps::FileHandlers* file_handlers =
      extensions::WebAppFileHandlers::GetWebAppFileHandlers(extension.get());

  ASSERT_TRUE(file_handlers);
  EXPECT_EQ(2u, file_handlers->size());

  {
    const apps::FileHandler& file_handler = file_handlers->at(0);
    EXPECT_EQ("https://graphr.n/open-graph/", file_handler.action);
    EXPECT_EQ(1u, file_handler.accept.size());
    EXPECT_EQ("text/svg+xml", file_handler.accept[0].mime_type);
    EXPECT_THAT(file_handler.accept[0].file_extensions,
                testing::UnorderedElementsAre(".svg"));
  }
  {
    const apps::FileHandler& file_handler = file_handlers->at(1);
    EXPECT_EQ("https://graphr.n/open-raw/", file_handler.action);
    EXPECT_EQ(1u, file_handler.accept.size());
    EXPECT_EQ("text/csv", file_handler.accept[0].mime_type);
    EXPECT_THAT(file_handler.accept[0].file_extensions,
                testing::UnorderedElementsAre(".csv"));
  }
}

// Tests that |shortcuts_menu_item_infos| on the WebAppManifest is correctly
// converted to |web_app_shortcut_icons| and |web_app_linked_shortcut_items| on
// an extension manifest.
TEST_F(ExtensionFromWebAppWithShortcutsMenu,
       WebAppShortcutIconsAreCorrectlyConverted) {
  StartExtensionService();
  WebApplicationInfo web_app;
  WebApplicationShortcutsMenuItemInfo shortcut_item;
  std::map<SquareSizePx, SkBitmap> shortcut_icon_bitmaps;
  web_app.title = base::ASCIIToUTF16("Shortcut App");
  web_app.description = base::ASCIIToUTF16("We have shortcuts.");
  web_app.start_url = GURL("https://shortcut-app.io/");
  web_app.scope = GURL("https://shortcut-app.io");

  shortcut_item.name = base::ASCIIToUTF16("Shortcut 1");
  shortcut_item.url = GURL("https://shortcut-app.io/shortcuts/shortcut1");
  {
    const int sizes[] = {16, 128};
    for (const auto& size : sizes) {
      WebApplicationShortcutsMenuItemInfo::Icon icon_info;
      icon_info.url = web_app.start_url.Resolve(
          base::StringPrintf("shortcut1/%i.png", size));
      icon_info.square_size_px = size;
      shortcut_item.shortcut_icon_infos.push_back(std::move(icon_info));
      shortcut_icon_bitmaps[size] = GetIconBitmap(size);
    }
    web_app.shortcuts_menu_icons_bitmaps.emplace_back(
        std::move(shortcut_icon_bitmaps));
  }
  web_app.shortcuts_menu_item_infos.push_back(std::move(shortcut_item));

  shortcut_item.name = base::ASCIIToUTF16("Shortcut 2");
  shortcut_item.url = GURL("https://shortcut-app.io/shortcuts/shortcut2");
  {
    const int sizes[] = {16, 48};
    for (const auto& size : sizes) {
      WebApplicationShortcutsMenuItemInfo::Icon icon_info;
      icon_info.url =
          web_app.start_url.Resolve(base::StringPrintf("0/%i.png", size));
      icon_info.square_size_px = size;
      shortcut_item.shortcut_icon_infos.push_back(std::move(icon_info));
      shortcut_icon_bitmaps[size] = GetIconBitmap(size);
    }
    web_app.shortcuts_menu_icons_bitmaps.emplace_back(
        std::move(shortcut_icon_bitmaps));
  }
  web_app.shortcuts_menu_item_infos.push_back(std::move(shortcut_item));

  scoped_refptr<Extension> extension = ConvertWebAppToExtension(
      web_app, GetTestTime(1978, 12, 11, 0, 0, 0, 0), ExtensionPath(),
      Extension::FROM_BOOKMARK, Manifest::INTERNAL);

  ASSERT_TRUE(extension.get());

  const WebAppLinkedShortcutItems& linked_shortcut_items =
      WebAppLinkedShortcutItems::GetWebAppLinkedShortcutItems(extension.get());
  const std::map<int, ExtensionIconSet>& shortcut_icons =
      WebAppShortcutIconsInfo::GetShortcutIcons(extension.get());
  for (size_t i = 0; i < web_app.shortcuts_menu_item_infos.size(); ++i) {
    const std::vector<WebApplicationShortcutsMenuItemInfo::Icon>& icon_infos =
        web_app.shortcuts_menu_item_infos[i].shortcut_icon_infos;
    const std::vector<WebAppLinkedShortcutItems::ShortcutItemInfo::IconInfo>&
        linked_shortcut_icons_info =
            linked_shortcut_items.shortcut_item_infos[i]
                .shortcut_item_icon_infos;
    ASSERT_EQ(icon_infos.size(), linked_shortcut_icons_info.size());
    for (size_t j = 0; j < icon_infos.size(); ++j) {
      EXPECT_EQ(linked_shortcut_icons_info[j].url, icon_infos[j].url);
      EXPECT_EQ(linked_shortcut_icons_info[j].size,
                icon_infos[j].square_size_px);
    }

    const std::map<SquareSizePx, SkBitmap>& icon_bitmaps =
        web_app.shortcuts_menu_icons_bitmaps[i];
    EXPECT_EQ(icon_bitmaps.size(), shortcut_icons.at(i).map().size());
    for (const std::pair<const SquareSizePx, SkBitmap>& icon : icon_bitmaps) {
      int size = icon.first;
      EXPECT_EQ(
          base::StringPrintf("shortcut_icons/%i/%i.png", static_cast<int>(i),
                             size),
          shortcut_icons.at(i).Get(size, ExtensionIconSet::MATCH_EXACTLY));

      ExtensionResource resource = WebAppShortcutIconsInfo::GetIconResource(
          extension.get(), i, size, ExtensionIconSet::MATCH_EXACTLY);
      EXPECT_TRUE(base::PathExists(resource.GetFilePath()));
      ASSERT_TRUE(!resource.empty());
    }
  }
}

}  // namespace extensions
