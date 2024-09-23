// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chooser_controller/title_util.h"

#include "base/command_line.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/navigation_simulator.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if !BUILDFLAG(IS_ANDROID)
#include "base/test/gmock_expected_support.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#endif  // !BUILDFLAG(IS_ANDROID)

namespace {

constexpr int kTitleResourceId = IDS_USB_DEVICE_CHOOSER_PROMPT;

using CreateChooserTitleTest = ChromeRenderViewHostTestHarness;

TEST_F(CreateChooserTitleTest, NoFrame) {
  EXPECT_EQ(u"", CreateChooserTitle(nullptr, kTitleResourceId));
}

TEST_F(CreateChooserTitleTest, UrlFrameTree) {
  NavigateAndCommit(GURL("https://main-frame.com"));
  content::RenderFrameHost* subframe =
      content::NavigationSimulator::NavigateAndCommitFromDocument(
          GURL("https://sub-frame.com"),
          content::RenderFrameHostTester::For(main_rfh())
              ->AppendChild("subframe"));

  EXPECT_EQ("main-frame.com", main_rfh()->GetLastCommittedOrigin().host());
  EXPECT_EQ(u"main-frame.com wants to connect",
            CreateChooserTitle(main_rfh(), kTitleResourceId));
  EXPECT_EQ("sub-frame.com", subframe->GetLastCommittedOrigin().host());
  EXPECT_EQ(u"main-frame.com wants to connect",
            CreateChooserTitle(subframe, kTitleResourceId));
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
TEST_F(CreateChooserTitleTest, ExtensionsFrameTree) {
  auto manifest = base::Value::Dict()
                      .Set("name", "Chooser Title Subframe Test")
                      .Set("version", "0.1")
                      .Set("manifest_version", 2)
                      .Set("web_accessible_resources",
                           base::Value::List().Append("index.html"));
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder().SetManifest(std::move(manifest)).Build();
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

  ASSERT_EQ(extension->id(), main_rfh()->GetLastCommittedOrigin().host());
  EXPECT_EQ(u"Chooser Title Subframe Test wants to connect",
            CreateChooserTitle(main_rfh(), kTitleResourceId));
  ASSERT_NE(extension->id(), subframe->GetLastCommittedOrigin().host());
  EXPECT_EQ(u"Chooser Title Subframe Test wants to connect",
            CreateChooserTitle(subframe, kTitleResourceId));
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if !BUILDFLAG(IS_ANDROID)
TEST_F(CreateChooserTitleTest, IsolatedWebAppFrameTree) {
  data_decoder::test::InProcessDataDecoder in_process_data_decoder;
  web_app::test::AwaitStartWebAppProviderAndSubsystems(profile());

  std::unique_ptr<web_app::ScopedBundledIsolatedWebApp> iwa =
      web_app::IsolatedWebAppBuilder(web_app::ManifestBuilder().SetName(
                                         "Chooser Title FrameTree IWA Name"))
          .BuildBundle();
  iwa->TrustSigningKey();
  iwa->FakeInstallPageState(profile());
  ASSERT_OK_AND_ASSIGN(web_app::IsolatedWebAppUrlInfo url_info,
                       iwa->Install(profile()));
  GURL app_url = url_info.origin().GetURL();
  web_app::SimulateIsolatedWebAppNavigation(web_contents(), app_url);

  content::RenderFrameHost* subframe =
      content::NavigationSimulator::NavigateAndCommitFromDocument(
          GURL("data:text/html,"),
          content::RenderFrameHostTester::For(main_rfh())
              ->AppendChild("subframe"));

  ASSERT_EQ(app_url, main_rfh()->GetLastCommittedOrigin().GetURL());
  EXPECT_EQ(u"Chooser Title FrameTree IWA Name wants to connect",
            CreateChooserTitle(main_rfh(), kTitleResourceId));
  ASSERT_NE(app_url, subframe->GetLastCommittedOrigin().GetURL());
  EXPECT_EQ(u"Chooser Title FrameTree IWA Name wants to connect",
            CreateChooserTitle(subframe, kTitleResourceId));
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace
