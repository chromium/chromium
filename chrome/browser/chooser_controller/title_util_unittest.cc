// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chooser_controller/title_util.h"

#include "base/command_line.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/strings/grit/components_strings.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "content/public/test/navigation_simulator.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/value_builder.h"
#include "url/gurl.h"
#include "url/origin.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

namespace {

constexpr int kNonExtensionTitleResourceId =
    IDS_USB_DEVICE_CHOOSER_PROMPT_ORIGIN;
constexpr int kExtensionTitleResourceId =
    IDS_USB_DEVICE_CHOOSER_PROMPT_EXTENSION_NAME;

using ExtensionsAwareChooserTitleTest = ChromeRenderViewHostTestHarness;

TEST_F(ExtensionsAwareChooserTitleTest, NoFrame) {
  EXPECT_EQ(u"", CreateExtensionAwareChooserTitle(nullptr,
                                                  kNonExtensionTitleResourceId,
                                                  kExtensionTitleResourceId));
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
TEST_F(ExtensionsAwareChooserTitleTest, FrameTree) {
  extensions::DictionaryBuilder manifest;
  manifest.Set("name", "Chooser Title Subframe Test")
      .Set("version", "0.1")
      .Set("manifest_version", 2)
      .Set("web_accessible_resources",
           extensions::ListBuilder().Append("index.html").Build());
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder().SetManifest(manifest.Build()).Build();
  ASSERT_TRUE(extension);

  extensions::TestExtensionSystem* extension_system =
      static_cast<extensions::TestExtensionSystem*>(
          extensions::ExtensionSystem::Get(profile()));
  extensions::ExtensionService* extension_service =
      extension_system->CreateExtensionService(
          base::CommandLine::ForCurrentProcess(), base::FilePath(), false);
  extension_service->AddExtension(extension.get());

  NavigateAndCommit(extension->GetResourceURL("index.html"));
  content::RenderFrameHost* subframe =
      content::NavigationSimulator::NavigateAndCommitFromDocument(
          GURL("data:text/html,"),
          content::RenderFrameHostTester::For(main_rfh())
              ->AppendChild("subframe"));

  EXPECT_EQ(extension->id(), main_rfh()->GetLastCommittedOrigin().host());
  EXPECT_EQ(
      u"\"Chooser Title Subframe Test\" wants to connect",
      CreateExtensionAwareChooserTitle(main_rfh(), kNonExtensionTitleResourceId,
                                       kExtensionTitleResourceId));
  EXPECT_NE(extension->id(), subframe->GetLastCommittedOrigin().host());
  EXPECT_EQ(
      u"\"Chooser Title Subframe Test\" wants to connect",
      CreateExtensionAwareChooserTitle(subframe, kNonExtensionTitleResourceId,
                                       kExtensionTitleResourceId));
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

}  // namespace
