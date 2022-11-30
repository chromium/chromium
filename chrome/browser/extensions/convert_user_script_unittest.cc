// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/convert_user_script.h"

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/chrome_paths.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/content_scripts_handler.h"
#include "extensions/common/mojom/run_location.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

static void AddPattern(URLPatternSet* extent, const std::string& pattern) {
  int schemes = URLPattern::SCHEME_ALL;
  extent->AddPattern(URLPattern(schemes, pattern));
}

}

class ExtensionFromUserScript : public testing::Test {
};

TEST_F(ExtensionFromUserScript, Basic) {
  base::ScopedTempDir extensions_dir;
  ASSERT_TRUE(extensions_dir.CreateUniqueTempDir());

  base::FilePath test_file;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_file));
  test_file = test_file.AppendASCII("extensions")
                       .AppendASCII("user_script_basic.user.js");

  std::u16string error;
  scoped_refptr<Extension> extension(
      ConvertUserScriptToExtension(test_file, GURL("http://www.google.com/foo"),
                                   extensions_dir.GetPath(), &error));

  ASSERT_TRUE(extension.get());
  EXPECT_EQ(std::u16string(), error);

  // Use a temp dir so that the extensions dir will clean itself up.
  base::ScopedTempDir ext_dir;
  EXPECT_TRUE(ext_dir.Set(extension->path()));

  // Validate generated extension metadata.
  EXPECT_EQ("My user script", extension->name());
  EXPECT_EQ("2.2.2", extension->VersionString());
  EXPECT_EQ("Does totally awesome stuff.", extension->description());
  EXPECT_EQ("IhCFCg9PMQTAcJdc9ytUP99WME+4yh6aMnM1uupkovo=",
            extension->public_key());
  EXPECT_EQ(Manifest::TYPE_USER_SCRIPT, extension->GetType());

  ASSERT_EQ(1u, ContentScriptsInfo::GetContentScripts(extension.get()).size());
  const UserScript& script =
      *ContentScriptsInfo::GetContentScripts(extension.get())[0];
  EXPECT_EQ(mojom::RunLocation::kDocumentIdle, script.run_location());
  ASSERT_EQ(2u, script.globs().size());
  EXPECT_EQ("http://www.google.com/*", script.globs().at(0));
  EXPECT_EQ("http://www.yahoo.com/*", script.globs().at(1));
  ASSERT_EQ(1u, script.exclude_globs().size());
  EXPECT_EQ("*foo*", script.exclude_globs().at(0));
  ASSERT_EQ(1u, script.url_patterns().patterns().size());
  EXPECT_EQ("http://www.google.com/*",
            script.url_patterns().begin()->GetAsString());
  ASSERT_EQ(1u, script.exclude_url_patterns().patterns().size());
  EXPECT_EQ("http://www.google.com/foo*",
            script.exclude_url_patterns().begin()->GetAsString());
  EXPECT_TRUE(script.emulate_greasemonkey());

  // Make sure the files actually exist on disk.
  EXPECT_TRUE(base::PathExists(
      extension->path().Append(script.js_scripts()[0]->relative_path())));
  EXPECT_TRUE(base::PathExists(
      extension->path().Append(kManifestFilename)));
}

TEST_F(ExtensionFromUserScript, NoMetadata) {
  base::ScopedTempDir extensions_dir;
  ASSERT_TRUE(extensions_dir.CreateUniqueTempDir());

  base::FilePath test_file;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_file));
  test_file = test_file.AppendASCII("extensions")
                       .AppendASCII("user_script_no_metadata.user.js");

  std::u16string error;
  scoped_refptr<Extension> extension(ConvertUserScriptToExtension(
      test_file, GURL("http://www.google.com/foo/bar.user.js?monkey"),
      extensions_dir.GetPath(), &error));

  ASSERT_TRUE(extension.get());
  EXPECT_EQ(std::u16string(), error);

  // Use a temp dir so that the extensions dir will clean itself up.
  base::ScopedTempDir ext_dir;
  EXPECT_TRUE(ext_dir.Set(extension->path()));

  // Validate generated extension metadata.
  EXPECT_EQ("bar.user.js", extension->name());
  EXPECT_EQ("1.0", extension->VersionString());
  EXPECT_EQ("", extension->description());
  EXPECT_EQ("k1WxKx54hX6tfl5gQaXD/m4d9QUMwRdXWM4RW+QkWcY=",
            extension->public_key());
  EXPECT_EQ(Manifest::TYPE_USER_SCRIPT, extension->GetType());

  ASSERT_EQ(1u, ContentScriptsInfo::GetContentScripts(extension.get()).size());
  const UserScript& script =
      *ContentScriptsInfo::GetContentScripts(extension.get())[0];
  ASSERT_EQ(1u, script.globs().size());
  EXPECT_EQ("*", script.globs()[0]);
  EXPECT_EQ(0u, script.exclude_globs().size());
  EXPECT_TRUE(script.emulate_greasemonkey());

  URLPatternSet expected;
  AddPattern(&expected, "http://*/*");
  AddPattern(&expected, "https://*/*");
  EXPECT_EQ(expected, script.url_patterns());

  // Make sure the files actually exist on disk.
  EXPECT_TRUE(base::PathExists(
      extension->path().Append(script.js_scripts()[0]->relative_path())));
  EXPECT_TRUE(base::PathExists(
      extension->path().Append(kManifestFilename)));
}

TEST_F(ExtensionFromUserScript, NotUTF8) {
  base::ScopedTempDir extensions_dir;
  ASSERT_TRUE(extensions_dir.CreateUniqueTempDir());

  base::FilePath test_file;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_file));
  test_file = test_file.AppendASCII("extensions")
                       .AppendASCII("user_script_not_utf8.user.js");

  std::u16string error;
  scoped_refptr<Extension> extension(ConvertUserScriptToExtension(
      test_file, GURL("http://www.google.com/foo/bar.user.js?monkey"),
      extensions_dir.GetPath(), &error));

  ASSERT_FALSE(extension.get());
  EXPECT_EQ(u"User script must be UTF8 encoded.", error);
}

TEST_F(ExtensionFromUserScript, RunAtDocumentStart) {
  base::ScopedTempDir extensions_dir;
  ASSERT_TRUE(extensions_dir.CreateUniqueTempDir());

  base::FilePath test_file;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_file));
  test_file = test_file.AppendASCII("extensions")
                       .AppendASCII("user_script_run_at_start.user.js");

  std::u16string error;
  scoped_refptr<Extension> extension(
      ConvertUserScriptToExtension(test_file, GURL("http://www.google.com/foo"),
                                   extensions_dir.GetPath(), &error));

  ASSERT_TRUE(extension.get());
  EXPECT_EQ(std::u16string(), error);

  // Use a temp dir so that the extensions dir will clean itself up.
  base::ScopedTempDir ext_dir;
  EXPECT_TRUE(ext_dir.Set(extension->path()));

  // Validate generated extension metadata.
  EXPECT_EQ("Document Start Test", extension->name());
  EXPECT_EQ("This script tests document-start", extension->description());
  EXPECT_EQ("RjmyI7+Gp/YHcW1qnu4xDxkJcL4cV4kTzdCA4BajCbk=",
            extension->public_key());
  EXPECT_EQ(Manifest::TYPE_USER_SCRIPT, extension->GetType());

  // Validate run location.
  ASSERT_EQ(1u, ContentScriptsInfo::GetContentScripts(extension.get()).size());
  const UserScript& script =
      *ContentScriptsInfo::GetContentScripts(extension.get())[0];
  EXPECT_EQ(mojom::RunLocation::kDocumentStart, script.run_location());
}

TEST_F(ExtensionFromUserScript, RunAtDocumentEnd) {
  base::ScopedTempDir extensions_dir;
  ASSERT_TRUE(extensions_dir.CreateUniqueTempDir());

  base::FilePath test_file;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_file));
  test_file = test_file.AppendASCII("extensions")
                       .AppendASCII("user_script_run_at_end.user.js");

  std::u16string error;
  scoped_refptr<Extension> extension(
      ConvertUserScriptToExtension(test_file, GURL("http://www.google.com/foo"),
                                   extensions_dir.GetPath(), &error));

  ASSERT_TRUE(extension.get());
  EXPECT_EQ(std::u16string(), error);

  // Use a temp dir so that the extensions dir will clean itself up.
  base::ScopedTempDir ext_dir;
  EXPECT_TRUE(ext_dir.Set(extension->path()));

  // Validate generated extension metadata.
  EXPECT_EQ("Document End Test", extension->name());
  EXPECT_EQ("This script tests document-end", extension->description());
  EXPECT_EQ("cpr5i8Mi24FzECV8UJe6tanwlU8SWesZosJ915YISvQ=",
            extension->public_key());
  EXPECT_EQ(Manifest::TYPE_USER_SCRIPT, extension->GetType());

  // Validate run location.
  ASSERT_EQ(1u, ContentScriptsInfo::GetContentScripts(extension.get()).size());
  const UserScript& script =
      *ContentScriptsInfo::GetContentScripts(extension.get())[0];
  EXPECT_EQ(mojom::RunLocation::kDocumentEnd, script.run_location());
}

TEST_F(ExtensionFromUserScript, RunAtDocumentIdle) {
  base::ScopedTempDir extensions_dir;
  ASSERT_TRUE(extensions_dir.CreateUniqueTempDir());

  base::FilePath test_file;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_file));
  test_file = test_file.AppendASCII("extensions")
                       .AppendASCII("user_script_run_at_idle.user.js");
  ASSERT_TRUE(base::PathExists(test_file)) << test_file.value();

  std::u16string error;
  scoped_refptr<Extension> extension(
      ConvertUserScriptToExtension(test_file, GURL("http://www.google.com/foo"),
                                   extensions_dir.GetPath(), &error));

  ASSERT_TRUE(extension.get());
  EXPECT_EQ(std::u16string(), error);

  // Use a temp dir so that the extensions dir will clean itself up.
  base::ScopedTempDir ext_dir;
  EXPECT_TRUE(ext_dir.Set(extension->path()));

  // Validate generated extension metadata.
  EXPECT_EQ("Document Idle Test", extension->name());
  EXPECT_EQ("This script tests document-idle", extension->description());
  EXPECT_EQ("kHnHKec3O/RKKo5/Iu1hKqe4wQERthL0639isNtsfiY=",
            extension->public_key());
  EXPECT_EQ(Manifest::TYPE_USER_SCRIPT, extension->GetType());

  // Validate run location.
  ASSERT_EQ(1u, ContentScriptsInfo::GetContentScripts(extension.get()).size());
  const UserScript& script =
      *ContentScriptsInfo::GetContentScripts(extension.get())[0];
  EXPECT_EQ(mojom::RunLocation::kDocumentIdle, script.run_location());
}

}  // namespace extensions
