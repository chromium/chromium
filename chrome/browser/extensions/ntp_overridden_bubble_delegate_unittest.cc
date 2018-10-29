// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/ntp_overridden_bubble_delegate.h"

#include <memory>

#include "base/bind.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/extension_web_ui_override_registrar.h"
#include "chrome/browser/profiles/profile.h"
#include "components/crx_file/id_util.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/value_builder.h"

namespace extensions {

namespace {

std::unique_ptr<KeyedService> BuildOverrideRegistrar(
    content::BrowserContext* context) {
  return std::make_unique<ExtensionWebUIOverrideRegistrar>(context);
}

scoped_refptr<const Extension> GetNtpExtension(const std::string& name) {
  return ExtensionBuilder()
      .SetManifest(
          DictionaryBuilder()
              .Set("name", name)
              .Set("version", "1.0")
              .Set("manifest_version", 2)
              .Set("chrome_url_overrides",
                   DictionaryBuilder().Set("newtab", "newtab.html").Build())
              .Build())
      .SetID(crx_file::id_util::GenerateId(name))
      .Build();
}

}  // namespace

using NtpOverriddenBubbleDelegateTest = ExtensionServiceTestBase;

TEST_F(NtpOverriddenBubbleDelegateTest, TestAcknowledgeExistingExtensions) {
  NtpOverriddenBubbleDelegate::set_acknowledge_existing_extensions_for_testing(
      true);

  InitializeEmptyExtensionService();
  ExtensionWebUIOverrideRegistrar::GetFactoryInstance()->SetTestingFactory(
      profile(), base::BindRepeating(&BuildOverrideRegistrar));
  // We need to trigger the instantiation of the WebUIOverrideRegistrar for
  // it to be constructed, since by default it's not constructed in tests.
  ExtensionWebUIOverrideRegistrar::GetFactoryInstance()->Get(profile());

  // Create an extension overriding the NTP.
  scoped_refptr<const Extension> first = GetNtpExtension("first");
  service()->AddExtension(first.get());

  auto include_extension = [this](const Extension* extension) {
    auto ntp_delegate =
        std::make_unique<NtpOverriddenBubbleDelegate>(profile());
    return ntp_delegate->ShouldIncludeExtension(extension);
  };
  // By default, we should warn about an extension overriding the NTP.
  EXPECT_TRUE(include_extension(first.get()));

  // Acknowledge existing extensions. Now, |first| should be acknowledged and
  // shouldn't be included in the bubble warning.
  NtpOverriddenBubbleDelegate::MaybeAcknowledgeExistingNtpExtensions(profile());
  EXPECT_FALSE(include_extension(first.get()));

  // Install a second NTP-overriding extension. As before, we should include the
  // extension.
  scoped_refptr<const Extension> second = GetNtpExtension("second");
  service()->AddExtension(second.get());
  EXPECT_TRUE(include_extension(second.get()));

  // Try acknowledging existing extensions. Since we already did this once for
  // this profile, this should have no effect, and we should still warn about
  // the second extension.
  NtpOverriddenBubbleDelegate::MaybeAcknowledgeExistingNtpExtensions(profile());
  EXPECT_TRUE(include_extension(second.get()));

  // We should still not warn about the first.
  EXPECT_FALSE(include_extension(first.get()));

  // Clean up.
  NtpOverriddenBubbleDelegate::set_acknowledge_existing_extensions_for_testing(
      false);
}

}  // namespace extensions
