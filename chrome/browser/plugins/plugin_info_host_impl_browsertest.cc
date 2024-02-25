// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/plugin_info_host_impl.h"

#include <stddef.h>

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/i18n/base_i18n_switches.h"
#include "base/i18n/rtl.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_move_support.h"
#include "base/test/mock_callback.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/chrome_browser_main_extra_parts_nacl_deprecation.h"
#include "chrome/browser/plugins/plugin_prefs.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/plugin.mojom.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/nacl/common/buildflags.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/webplugininfo.h"
#include "content/public/test/browser_test.h"
#include "pdf/buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(ENABLE_NACL)
#include "components/nacl/common/nacl_constants.h"
#include "ppapi/shared_impl/ppapi_permissions.h"
#endif  // BUILDFLAG(ENABLE_NACL)

#if BUILDFLAG(ENABLE_PDF)
#include "components/pdf/common/constants.h"
#endif  // BUILDFLAG(ENABLE_PDF)

namespace {

using ::chrome::mojom::PluginInfo;
using ::chrome::mojom::PluginInfoHost;
using ::chrome::mojom::PluginInfoPtr;
using ::chrome::mojom::PluginStatus;
using ::content::WebPluginInfo;
using ::content::WebPluginMimeType;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::SizeIs;

}  // namespace

class PluginInfoHostImplTest : public InProcessBrowserTest {
 public:
  PluginInfoHostImplTest() { feature_list_.InitAndEnableFeature(kNaclAllow); }

  void SetUpOnMainThread() override {
    int active_render_process_id = browser()
                                       ->tab_strip_model()
                                       ->GetActiveWebContents()
                                       ->GetPrimaryMainFrame()
                                       ->GetProcess()
                                       ->GetID();

    plugin_info_host_impl_ = std::make_unique<PluginInfoHostImpl>(
        active_render_process_id, browser()->profile());
  }

  void TearDownOnMainThread() override { plugin_info_host_impl_.reset(); }

 protected:
  PluginInfoPtr GetPluginInfo(const GURL& url,
                              const url::Origin& origin,
                              const std::string& mime_type) {
    PluginInfoPtr plugin_info;

    base::MockCallback<PluginInfoHost::GetPluginInfoCallback> mock_callback;
    EXPECT_CALL(mock_callback, Run).WillOnce(MoveArg(&plugin_info));

    base::RunLoop run_loop;
    plugin_info_host_impl_->GetPluginInfo(
        url, origin, mime_type,
        mock_callback.Get().Then(run_loop.QuitClosure()));
    run_loop.Run();

    return plugin_info;
  }

  void SetAlwaysOpenPdfExternally() {
    PluginPrefs::GetForProfile(browser()->profile())
        ->SetAlwaysOpenPdfExternallyForTests(true);
  }

  std::unique_ptr<PluginInfoHostImpl> plugin_info_host_impl_;
  base::test::ScopedFeatureList feature_list_;
};

#if BUILDFLAG(ENABLE_PDF)
// Variation that tests under left-to-right and right-to-left directions. The
// direction affects the PDF viewer extension, as the plugin name is derived
// from the extension name, and the extension name may be adjusted to include
// Unicode bidirectional control characters in RTL mode. These extra control
// characters can break string comparisons (see crbug.com/1404260).
class PluginInfoHostImplBidiTest : public PluginInfoHostImplTest,
                                   public testing::WithParamInterface<bool> {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    if (GetParam()) {
      // Pass "--force-ui-direction=rtl" instead of setting an RTL locale, as
      // setting a locale requires extra code on GLib platforms. Setting the UI
      // direction has the same effect on the extension name, which is all
      // that's needed for these tests.
      command_line->AppendSwitchASCII(switches::kForceUIDirection,
                                      switches::kForceDirectionRTL);
    }
  }
};
#endif  // BUILDFLAG(ENABLE_PDF)

IN_PROC_BROWSER_TEST_F(PluginInfoHostImplTest, CoverAllPlugins) {
  // Note that "internal" plugins are the only type that can be registered with
  // `content::PluginService` now.
  std::vector<WebPluginInfo> plugins;
  content::PluginService::GetInstance()->GetInternalPlugins(&plugins);

  size_t expected_plugin_count = 0;

#if BUILDFLAG(ENABLE_NACL)
  EXPECT_THAT(plugins, Contains(Field(
                           "path", &WebPluginInfo::path,
                           base::FilePath(nacl::kInternalNaClPluginFileName))));
  expected_plugin_count += 1;
#endif  // BUILDFLAG_ENABLE_NACL)

#if BUILDFLAG(ENABLE_PDF)
  EXPECT_THAT(
      plugins,
      Contains(
          Field("path", &WebPluginInfo::path,
                base::FilePath(ChromeContentClient::kPDFExtensionPluginPath))));
  EXPECT_THAT(
      plugins,
      Contains(
          Field("path", &WebPluginInfo::path,
                base::FilePath(ChromeContentClient ::kPDFInternalPluginPath))));
  expected_plugin_count += 2;
#endif  // BUILDFLAG(ENABLE_PDF)

  EXPECT_THAT(plugins, SizeIs(expected_plugin_count));
}

IN_PROC_BROWSER_TEST_F(PluginInfoHostImplTest, GetPluginInfoForFlash) {
  PluginInfoPtr plugin_info = GetPluginInfo(GURL("fake.swf"), url::Origin(),
                                            "application/x-shockwave-flash");
  ASSERT_TRUE(plugin_info);

  EXPECT_EQ(PluginStatus::kNotFound, plugin_info->status);
}

IN_PROC_BROWSER_TEST_F(PluginInfoHostImplTest, GetPluginInfoForFutureSplash) {
  PluginInfoPtr plugin_info = GetPluginInfo(GURL("fake.spl"), url::Origin(),
                                            "application/futuresplash");
  ASSERT_TRUE(plugin_info);

  EXPECT_EQ(PluginStatus::kNotFound, plugin_info->status);
}

#if BUILDFLAG(ENABLE_NACL)
IN_PROC_BROWSER_TEST_F(PluginInfoHostImplTest, GetPluginInfoForNaCl) {
  const std::u16string kPluginName = base::UTF8ToUTF16(nacl::kNaClPluginName);
  const base::FilePath kPluginPath =
      base::FilePath(nacl::kInternalNaClPluginFileName);

  PluginInfoPtr plugin_info = GetPluginInfo(
      GURL("fake-resource"), url::Origin(), nacl::kNaClPluginMimeType);
  ASSERT_TRUE(plugin_info);

  EXPECT_EQ(PluginStatus::kPlayImportantContent, plugin_info->status);
  EXPECT_EQ(nacl::kNaClPluginMimeType, plugin_info->actual_mime_type);

  // Group ID and name synthesized by `PluginInfoHostImpl`.
  EXPECT_EQ(kPluginPath.BaseName().AsUTF8Unsafe(),
            plugin_info->group_identifier);
  EXPECT_EQ(kPluginName, plugin_info->group_name);

  // `WebPluginInfo` fields.
  EXPECT_EQ(kPluginName, plugin_info->plugin.name);
  EXPECT_EQ(kPluginPath, plugin_info->plugin.path);
  EXPECT_EQ(u"", plugin_info->plugin.version);
  EXPECT_EQ(u"", plugin_info->plugin.desc);
  EXPECT_EQ(WebPluginInfo::PLUGIN_TYPE_PEPPER_IN_PROCESS,
            plugin_info->plugin.type);
  EXPECT_EQ(ppapi::PERMISSION_PRIVATE | ppapi::PERMISSION_DEV,
            plugin_info->plugin.pepper_permissions);
  EXPECT_EQ(WebPluginInfo::kDefaultBackgroundColor,
            plugin_info->plugin.background_color);

  // Has both NaCl and Pnacl MIME types.
  ASSERT_THAT(plugin_info->plugin.mime_types, SizeIs(2));

  WebPluginMimeType nacl_mime_type = plugin_info->plugin.mime_types[0];
  EXPECT_EQ(nacl::kNaClPluginMimeType, nacl_mime_type.mime_type);
  EXPECT_THAT(nacl_mime_type.file_extensions,
              ElementsAre(nacl::kNaClPluginExtension));
  EXPECT_EQ(base::UTF8ToUTF16(nacl::kNaClPluginDescription),
            nacl_mime_type.description);

  // Depends on modules registered by `extensions::NaClModulesHandler`.
  EXPECT_THAT(nacl_mime_type.additional_params, IsEmpty());

  WebPluginMimeType pnacl_mime_type = plugin_info->plugin.mime_types[1];
  EXPECT_EQ(nacl::kPnaclPluginMimeType, pnacl_mime_type.mime_type);
  EXPECT_THAT(pnacl_mime_type.file_extensions,
              ElementsAre(nacl::kPnaclPluginExtension));
  EXPECT_EQ(base::UTF8ToUTF16(nacl::kPnaclPluginDescription),
            pnacl_mime_type.description);
  EXPECT_THAT(pnacl_mime_type.additional_params, IsEmpty());
}

IN_PROC_BROWSER_TEST_F(PluginInfoHostImplTest, GetPluginInfoForPnacl) {
  const std::u16string kPluginName = base::UTF8ToUTF16(nacl::kNaClPluginName);
  const base::FilePath kPluginPath =
      base::FilePath(nacl::kInternalNaClPluginFileName);

  PluginInfoPtr plugin_info = GetPluginInfo(
      GURL("fake-resource"), url::Origin(), nacl::kPnaclPluginMimeType);
  ASSERT_TRUE(plugin_info);

  EXPECT_EQ(PluginStatus::kPlayImportantContent, plugin_info->status);
  EXPECT_EQ(nacl::kPnaclPluginMimeType, plugin_info->actual_mime_type);

  // Group ID and name synthesized by `PluginInfoHostImpl`.
  EXPECT_EQ(kPluginPath.BaseName().AsUTF8Unsafe(),
            plugin_info->group_identifier);
  EXPECT_EQ(kPluginName, plugin_info->group_name);

  // `WebPluginInfo` fields.
  EXPECT_EQ(kPluginName, plugin_info->plugin.name);
  EXPECT_EQ(kPluginPath, plugin_info->plugin.path);
  EXPECT_EQ(u"", plugin_info->plugin.version);
  EXPECT_EQ(u"", plugin_info->plugin.desc);
  EXPECT_EQ(WebPluginInfo::PLUGIN_TYPE_PEPPER_IN_PROCESS,
            plugin_info->plugin.type);
  EXPECT_EQ(ppapi::PERMISSION_PRIVATE | ppapi::PERMISSION_DEV,
            plugin_info->plugin.pepper_permissions);
  EXPECT_EQ(WebPluginInfo::kDefaultBackgroundColor,
            plugin_info->plugin.background_color);

  // Has both NaCl and Pnacl MIME types.
  ASSERT_THAT(plugin_info->plugin.mime_types, SizeIs(2));

  WebPluginMimeType nacl_mime_type = plugin_info->plugin.mime_types[0];
  EXPECT_EQ(nacl::kNaClPluginMimeType, nacl_mime_type.mime_type);
  EXPECT_THAT(nacl_mime_type.file_extensions,
              ElementsAre(nacl::kNaClPluginExtension));
  EXPECT_EQ(base::UTF8ToUTF16(nacl::kNaClPluginDescription),
            nacl_mime_type.description);

  // Depends on modules registered by `extensions::NaClModulesHandler`.
  EXPECT_THAT(nacl_mime_type.additional_params, IsEmpty());

  WebPluginMimeType pnacl_mime_type = plugin_info->plugin.mime_types[1];
  EXPECT_EQ(nacl::kPnaclPluginMimeType, pnacl_mime_type.mime_type);
  EXPECT_THAT(pnacl_mime_type.file_extensions,
              ElementsAre(nacl::kPnaclPluginExtension));
  EXPECT_EQ(base::UTF8ToUTF16(nacl::kPnaclPluginDescription),
            pnacl_mime_type.description);

  EXPECT_THAT(pnacl_mime_type.additional_params, IsEmpty());
}
#endif  // BUILDFLAG(ENABLE_NACL)

#if BUILDFLAG(ENABLE_PDF)
IN_PROC_BROWSER_TEST_P(PluginInfoHostImplBidiTest,
                       GetPluginInfoForPdfViewerExtension) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  const std::u16string kPluginName = u"Chrome PDF Viewer";
  const std::string kGroupId = "google-chrome-pdf";
#else
  const std::u16string kPluginName = u"Chromium PDF Viewer";
  const std::string kGroupId = "chromium-pdf";
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

  PluginInfoPtr plugin_info =
      GetPluginInfo(GURL("fake.pdf"), url::Origin(), pdf::kPDFMimeType);
  ASSERT_TRUE(plugin_info);

  EXPECT_EQ(PluginStatus::kAllowed, plugin_info->status);
  EXPECT_EQ(pdf::kPDFMimeType, plugin_info->actual_mime_type);

  // Group ID and name defined by `PluginInfoHostImpl`.
  EXPECT_EQ(kGroupId, plugin_info->group_identifier);
  EXPECT_EQ(kPluginName, plugin_info->group_name);

  // `WebPluginInfo` fields.
  std::u16string expected_plugin_name = kPluginName;
  if (GetParam()) {
    // Extra characters are added by `extensions::Extension::LoadName()`.
    ASSERT_TRUE(
        base::i18n::AdjustStringForLocaleDirection(&expected_plugin_name));
  }
  EXPECT_EQ(expected_plugin_name, plugin_info->plugin.name);

  EXPECT_EQ(base::FilePath(ChromeContentClient::kPDFExtensionPluginPath),
            plugin_info->plugin.path);
  EXPECT_EQ(u"", plugin_info->plugin.version);
  EXPECT_EQ(u"", plugin_info->plugin.desc);
  EXPECT_EQ(WebPluginInfo::PLUGIN_TYPE_BROWSER_PLUGIN,
            plugin_info->plugin.type);
  EXPECT_EQ(0, plugin_info->plugin.pepper_permissions);

  // Background color hard-coded in `MimeTypesHandler::GetBackgroundColor()`.
  EXPECT_EQ(SkColorSetRGB(82, 86, 89), plugin_info->plugin.background_color);

  // Has PDF MIME type.
  ASSERT_THAT(plugin_info->plugin.mime_types, SizeIs(1));

  WebPluginMimeType mime_type = plugin_info->plugin.mime_types[0];
  EXPECT_EQ(pdf::kPDFMimeType, mime_type.mime_type);
  EXPECT_THAT(mime_type.file_extensions, ElementsAre("pdf"));
  EXPECT_EQ(u"", mime_type.description);
  EXPECT_THAT(mime_type.additional_params, IsEmpty());
}

IN_PROC_BROWSER_TEST_P(PluginInfoHostImplBidiTest,
                       GetPluginInfoForPdfViewerExtensionWhenDisabled) {
  SetAlwaysOpenPdfExternally();

  PluginInfoPtr plugin_info =
      GetPluginInfo(GURL("fake.pdf"), url::Origin(), pdf::kPDFMimeType);
  ASSERT_TRUE(plugin_info);

  // PDF viewer extension is disabled by PDF content setting.
  EXPECT_EQ(PluginStatus::kDisabled, plugin_info->status);
  EXPECT_EQ(pdf::kPDFMimeType, plugin_info->actual_mime_type);
}

IN_PROC_BROWSER_TEST_F(PluginInfoHostImplTest,
                       GetPluginInfoForPdfInternalPlugin) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  const std::u16string kPluginName = u"Chrome PDF Plugin";
  const std::string kGroupId = "google-chrome-pdf-plugin";
#else
  const std::u16string kPluginName = u"Chromium PDF Plugin";
  const std::string kGroupId = "chromium-pdf-plugin";
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

  PluginInfoPtr plugin_info = GetPluginInfo(GURL("fake.pdf"), url::Origin(),
                                            pdf::kInternalPluginMimeType);
  ASSERT_TRUE(plugin_info);

  EXPECT_EQ(PluginStatus::kAllowed, plugin_info->status);
  EXPECT_EQ(pdf::kInternalPluginMimeType, plugin_info->actual_mime_type);

  // Group ID and name defined by `PluginInfoHostImpl`.
  EXPECT_EQ(kGroupId, plugin_info->group_identifier);
  EXPECT_EQ(kPluginName, plugin_info->group_name);

  // `WebPluginInfo` fields.
  EXPECT_EQ(kPluginName, plugin_info->plugin.name);
  EXPECT_EQ(base::FilePath(ChromeContentClient::kPDFInternalPluginPath),
            plugin_info->plugin.path);
  EXPECT_EQ(u"", plugin_info->plugin.version);
  EXPECT_EQ(u"Portable Document Format", plugin_info->plugin.desc);
  EXPECT_EQ(WebPluginInfo::PLUGIN_TYPE_PEPPER_OUT_OF_PROCESS,
            plugin_info->plugin.type);
  EXPECT_EQ(0, plugin_info->plugin.pepper_permissions);
  EXPECT_EQ(WebPluginInfo::kDefaultBackgroundColor,
            plugin_info->plugin.background_color);

  // Has PDF MIME type.
  ASSERT_THAT(plugin_info->plugin.mime_types, SizeIs(1));

  WebPluginMimeType mime_type = plugin_info->plugin.mime_types[0];
  EXPECT_EQ(pdf::kInternalPluginMimeType, mime_type.mime_type);
  EXPECT_THAT(mime_type.file_extensions, ElementsAre("pdf"));
  EXPECT_EQ(u"Portable Document Format", mime_type.description);
  EXPECT_THAT(mime_type.additional_params, IsEmpty());
}

IN_PROC_BROWSER_TEST_F(PluginInfoHostImplTest,
                       GetPluginInfoForPdfInternalPluginWhenDisabled) {
  SetAlwaysOpenPdfExternally();

  PluginInfoPtr plugin_info = GetPluginInfo(GURL("fake.pdf"), url::Origin(),
                                            pdf::kInternalPluginMimeType);
  ASSERT_TRUE(plugin_info);

  // Internal PDF plugin is not affected by PDF content setting.
  EXPECT_EQ(PluginStatus::kAllowed, plugin_info->status);
  EXPECT_EQ(pdf::kInternalPluginMimeType, plugin_info->actual_mime_type);
}

INSTANTIATE_TEST_SUITE_P(All, PluginInfoHostImplBidiTest, testing::Bool());
#endif  // BUILDFLAG(ENABLE_PDF)
