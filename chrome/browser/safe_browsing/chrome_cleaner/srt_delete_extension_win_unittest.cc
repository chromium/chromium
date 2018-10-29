// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_cleaner/srt_chrome_prompt_impl.h"

#include "base/location.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/test_extension_service.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/mock_extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_set.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

class ExtensionDeletionTest : public extensions::ExtensionServiceTestBase {
 public:
  ExtensionDeletionTest() { InitializeEmptyExtensionService(); }
  ~ExtensionDeletionTest() override = default;

  void SetUp() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ExtensionDeletionTest);
};

TEST_F(ExtensionDeletionTest, DisableExtensionTest) {
  std::vector<base::string16> extension_ids{};
  extensions::ExtensionService* extension_service = this->service();
  for (int i = 40; i < 43; i++) {
    scoped_refptr<const extensions::Extension> extension =
        extensions::ExtensionBuilder(base::NumberToString(i))
            .SetManifestKey("version", "1")
            .Build();
    auto id = extension->id();
    extension_ids.push_back(base::UTF8ToUTF16(id));
    extension_service->AddExtension(extension.get());
    extension_service->EnableExtension(id);
  }
  std::unique_ptr<ChromePromptImpl> chrome_prompt =
      std::make_unique<ChromePromptImpl>(extension_service, nullptr,
                                         base::DoNothing(), base::DoNothing());
  chrome_prompt->PromptUser({}, {}, extension_ids, base::DoNothing());
  std::vector<base::string16> extensions_to_disable{extension_ids[0]};
  chrome_prompt->DisableExtensions(
      extensions_to_disable,
      base::BindOnce([](bool result) { EXPECT_TRUE(result); }));
  EXPECT_EQ(extension_service->GetInstalledExtension(
                base::UTF16ToUTF8(extension_ids[0])),
            nullptr);
  EXPECT_NE(extension_service->GetInstalledExtension(
                base::UTF16ToUTF8(extension_ids[1])),
            nullptr);
  EXPECT_NE(extension_service->GetInstalledExtension(
                base::UTF16ToUTF8(extension_ids[2])),
            nullptr);

  chrome_prompt = std::make_unique<ChromePromptImpl>(
      extension_service, nullptr, base::DoNothing(), base::DoNothing());
  chrome_prompt->PromptUser({}, {}, extension_ids, base::DoNothing());
  extensions_to_disable = {extension_ids[2], extension_ids[1]};
  chrome_prompt->DisableExtensions(
      extensions_to_disable,
      base::BindOnce([](bool result) { EXPECT_TRUE(result); }));
  EXPECT_EQ(extension_service->GetInstalledExtension(
                base::UTF16ToUTF8(extension_ids[0])),
            nullptr);
  EXPECT_EQ(extension_service->GetInstalledExtension(
                base::UTF16ToUTF8(extension_ids[1])),
            nullptr);
  EXPECT_EQ(extension_service->GetInstalledExtension(
                base::UTF16ToUTF8(extension_ids[2])),
            nullptr);
}

TEST_F(ExtensionDeletionTest, CantDeleteNonPromptedExtensions) {
  std::vector<base::string16> extension_ids{};
  extensions::ExtensionService* extension_service = this->service();
  for (int i = 40; i < 43; i++) {
    scoped_refptr<const extensions::Extension> extension =
        extensions::ExtensionBuilder(base::NumberToString(i))
            .SetManifestKey("version", "1")
            .Build();
    auto id = extension->id();
    extension_ids.push_back(base::UTF8ToUTF16(id));
    extension_service->AddExtension(extension.get());
    extension_service->EnableExtension(id);
  }
  std::unique_ptr<ChromePromptImpl> chrome_prompt =
      std::make_unique<ChromePromptImpl>(extension_service, nullptr,
                                         base::DoNothing(), base::DoNothing());
  std::vector<base::string16> extensions_to_disable{extension_ids[0]};
  chrome_prompt->DisableExtensions(
      extensions_to_disable,
      base::BindOnce([](bool result) { EXPECT_FALSE(result); }));
  EXPECT_NE(extension_service->GetInstalledExtension(
                base::UTF16ToUTF8(extension_ids[0])),
            nullptr);
  EXPECT_NE(extension_service->GetInstalledExtension(
                base::UTF16ToUTF8(extension_ids[1])),
            nullptr);
  EXPECT_NE(extension_service->GetInstalledExtension(
                base::UTF16ToUTF8(extension_ids[2])),
            nullptr);

  chrome_prompt = std::make_unique<ChromePromptImpl>(
      extension_service, nullptr, base::DoNothing(), base::DoNothing());
  chrome_prompt->DisableExtensions(
      extension_ids, base::BindOnce([](bool result) { EXPECT_FALSE(result); }));
  EXPECT_NE(extension_service->GetInstalledExtension(
                base::UTF16ToUTF8(extension_ids[0])),
            nullptr);
  EXPECT_NE(extension_service->GetInstalledExtension(
                base::UTF16ToUTF8(extension_ids[1])),
            nullptr);
  EXPECT_NE(extension_service->GetInstalledExtension(
                base::UTF16ToUTF8(extension_ids[2])),
            nullptr);

  chrome_prompt->PromptUser({}, {}, {{extension_ids[2]}}, base::DoNothing());
  chrome_prompt->DisableExtensions(
      {extension_ids[0], extension_ids[1]},
      base::BindOnce([](bool result) { EXPECT_FALSE(result); }));
  EXPECT_NE(extension_service->GetInstalledExtension(
                base::UTF16ToUTF8(extension_ids[0])),
            nullptr);
  EXPECT_NE(extension_service->GetInstalledExtension(
                base::UTF16ToUTF8(extension_ids[1])),
            nullptr);
  EXPECT_NE(extension_service->GetInstalledExtension(
                base::UTF16ToUTF8(extension_ids[2])),
            nullptr);
}

TEST_F(ExtensionDeletionTest, EmptyDeletionTest) {
  std::vector<base::string16> extension_ids{};
  extensions::ExtensionService* extension_service = this->service();
  std::unique_ptr<ChromePromptImpl> chrome_prompt =
      std::make_unique<ChromePromptImpl>(extension_service, nullptr,
                                         base::DoNothing(), base::DoNothing());
  chrome_prompt->PromptUser({}, {}, extension_ids, base::DoNothing());
  for (int i = 40; i < 43; i++) {
    scoped_refptr<const extensions::Extension> extension =
        extensions::ExtensionBuilder(base::NumberToString(i))
            .SetManifestKey("version", "1")
            .Build();
    auto id = extension->id();
    extension_ids.push_back(base::UTF8ToUTF16(id));
    extension_service->AddExtension(extension.get());
    extension_service->EnableExtension(id);
  }
  chrome_prompt->DisableExtensions(
      {}, base::BindOnce([](bool result) { EXPECT_TRUE(result); }));
  EXPECT_TRUE(extension_service->IsExtensionEnabled(
      base::UTF16ToUTF8(extension_ids[0])));
  EXPECT_TRUE(extension_service->IsExtensionEnabled(
      base::UTF16ToUTF8(extension_ids[1])));
  EXPECT_TRUE(extension_service->IsExtensionEnabled(
      base::UTF16ToUTF8(extension_ids[2])));
}

TEST_F(ExtensionDeletionTest, BadlyFormattedDeletionTest) {
  std::vector<base::string16> extension_ids{};
  extensions::ExtensionService* extension_service = this->service();
  std::unique_ptr<ChromePromptImpl> chrome_prompt =
      std::make_unique<ChromePromptImpl>(extension_service, nullptr,
                                         base::DoNothing(), base::DoNothing());
  for (int i = 40; i < 43; i++) {
    scoped_refptr<const extensions::Extension> extension =
        extensions::ExtensionBuilder(base::NumberToString(i))
            .SetManifestKey("version", "1")
            .Build();
    auto id = extension->id();
    extension_ids.push_back(base::UTF8ToUTF16(id));
    extension_service->AddExtension(extension.get());
    extension_service->EnableExtension(id);
  }
  chrome_prompt->DisableExtensions(
      {L"bad-extension-id"},
      base::BindOnce([](bool result) { EXPECT_FALSE(result); }));
  chrome_prompt->DisableExtensions(
      {L""}, base::BindOnce([](bool result) { EXPECT_FALSE(result); }));
  chrome_prompt->DisableExtensions(
      {L"🤷☝¯\\_(ツ)_/¯✌🤷"},
      base::BindOnce([](bool result) { EXPECT_FALSE(result); }));
}

TEST_F(ExtensionDeletionTest, NotInstalledExtensionTest) {
  std::vector<base::string16> extension_ids{};
  extensions::ExtensionService* extension_service = this->service();
  std::unique_ptr<ChromePromptImpl> chrome_prompt =
      std::make_unique<ChromePromptImpl>(extension_service, nullptr,
                                         base::DoNothing(), base::DoNothing());
  for (int i = 40; i < 43; i++) {
    // Don't actually install the extension
    scoped_refptr<const extensions::Extension> extension =
        extensions::ExtensionBuilder(base::NumberToString(i))
            .SetManifestKey("version", "1")
            .Build();
    auto id = extension->id();
    extension_ids.push_back(base::UTF8ToUTF16(id));
  }
  chrome_prompt->DisableExtensions(
      extension_ids, base::BindOnce([](bool result) { EXPECT_FALSE(result); }));
}

}  // namespace safe_browsing
