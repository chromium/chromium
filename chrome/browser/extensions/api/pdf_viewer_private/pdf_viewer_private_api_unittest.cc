// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/pdf_viewer_private/pdf_viewer_private_api.h"

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/values_test_util.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/pdf/pdf_test_util.h"
#include "chrome/browser/pdf/pdf_viewer_stream_manager.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/pdf/common/constants.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"
#include "pdf/buildflags.h"
#include "pdf/pdf_features.h"

#if BUILDFLAG(ENABLE_PDF_SAVE_TO_DRIVE)
#include "chrome/browser/save_to_drive/content_reader.h"
#include "chrome/browser/save_to_drive/save_to_drive_event_dispatcher.h"
#include "chrome/browser/save_to_drive/save_to_drive_flow.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/mock_hats_service.h"
#include "chrome/browser/ui/save_to_drive/get_account.h"
#include "chrome/common/extensions/api/pdf_viewer_private.h"
#include "extensions/browser/test_event_router.h"
#endif  // BUILDFLAG(ENABLE_PDF_SAVE_TO_DRIVE)

namespace extensions {

namespace {

constexpr char kSampleSetPdfPluginAttributesArgs[] = R"([{
  "backgroundColor": 10.0,
  "allowJavascript": false,
}])";

#if BUILDFLAG(ENABLE_PDF_SAVE_TO_DRIVE)
using save_to_drive::SaveToDriveFlow;

class MockSaveToDriveFlow : public SaveToDriveFlow {
 public:
  MockSaveToDriveFlow(
      content::RenderFrameHost* render_frame_host,
      std::unique_ptr<save_to_drive::SaveToDriveEventDispatcher>
          event_dispatcher,
      std::unique_ptr<save_to_drive::ContentReader> content_reader,
      std::unique_ptr<save_to_drive::AccountChooser> account_chooser,
      HatsService* hats_service)
      : SaveToDriveFlow(render_frame_host,
                        std::move(event_dispatcher),
                        std::move(content_reader),
                        std::move(account_chooser),
                        hats_service) {}
  MOCK_METHOD(void, Run, (), (override));
  MOCK_METHOD(void, Stop, (), (override));
};
#endif  // BUILDFLAG(ENABLE_PDF_SAVE_TO_DRIVE)

}  // namespace

class PdfViewerPrivateApiUnitTest : public ChromeRenderViewHostTestHarness {
 public:
  PdfViewerPrivateApiUnitTest() = default;

  PdfViewerPrivateApiUnitTest(const PdfViewerPrivateApiUnitTest&) = delete;
  PdfViewerPrivateApiUnitTest& operator=(const PdfViewerPrivateApiUnitTest&) =
      delete;

  ~PdfViewerPrivateApiUnitTest() override = default;

 protected:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(chrome_pdf::features::kPdfOopif);
    ChromeRenderViewHostTestHarness::SetUp();

    pdf::PdfViewerStreamManager::Create(web_contents());

    // For testing purposes, `main_rfh()` represents the extension's
    // embedder's frame host, while `extension_host` represents the
    // extension's frame host. The embedder's frame host is the parent of the
    // extension's frame host.
    auto* main_host_tester = content::RenderFrameHostTester::For(main_rfh());
    main_host_tester->InitializeRenderFrameIfNeeded();
    extension_host_ = main_host_tester->AppendChild("extension_host");
#if BUILDFLAG(ENABLE_PDF_SAVE_TO_DRIVE)
    event_router_ = extensions::CreateAndUseTestEventRouter(browser_context());
    create_save_to_drive_flow_callback_ =
        base::BindRepeating(&PdfViewerPrivateApiUnitTest::CreateSaveToDriveFlow,
                            base::Unretained(this));
    SaveToDriveFlow::SetCreateCallbackForTesting(
        &create_save_to_drive_flow_callback_);
#endif  // BUILDFLAG(ENABLE_PDF_SAVE_TO_DRIVE)
  }

  void TearDown() override {
    extension_host_ = nullptr;
#if BUILDFLAG(ENABLE_PDF_SAVE_TO_DRIVE)
    SaveToDriveFlow::SetCreateCallbackForTesting(nullptr);
    create_save_to_drive_flow_callback_.Reset();
    flow_ = nullptr;
    event_router_ = nullptr;
    hats_service_ = nullptr;
#endif  // BUILDFLAG(ENABLE_PDF_SAVE_TO_DRIVE)

    web_contents()->RemoveUserData(pdf::PdfViewerStreamManager::UserDataKey());
    ChromeRenderViewHostTestHarness::TearDown();
  }

  pdf::PdfViewerStreamManager* pdf_viewer_stream_manager() {
    return pdf::PdfViewerStreamManager::FromWebContents(web_contents());
  }

  content::RenderFrameHost* extension_host() { return extension_host_; }

#if BUILDFLAG(ENABLE_PDF_SAVE_TO_DRIVE)
  HatsService* hats_service() {
    if (!hats_service_) {
      hats_service_ = static_cast<MockHatsService*>(
          HatsServiceFactory::GetInstance()->SetTestingFactoryAndUse(
              profile(), base::BindRepeating(&BuildMockHatsService)));
    }
    return hats_service_;
  }
#endif  // BUILDFLAG(ENABLE_PDF_SAVE_TO_DRIVE)

  // Create a claimed stream container in `pdf::PdfViewerStreamManager`. This
  // updates `extension_host_`, since the navigation deletes the embedder frame
  // host's child frame hosts.
  void CreateAndClaimStreamContainer() {
    extension_host_ = nullptr;

    content::RenderFrameHost* embedder_host =
        content::NavigationSimulator::NavigateAndCommitFromDocument(
            GURL("https://original_url1"), main_rfh());
    pdf::PdfViewerStreamManager::Create(web_contents());

    auto* manager = pdf_viewer_stream_manager();
    manager->AddStreamContainer(
        embedder_host->GetFrameTreeNodeId(), "internal_id",
        pdf_test_util::GenerateSampleStreamContainer(1));
    manager->ClaimStreamInfoForTesting(embedder_host);

    // After navigation, the extension host needs to be appended again.
    auto* embedder_host_tester =
        content::RenderFrameHostTester::For(embedder_host);
    embedder_host_tester->InitializeRenderFrameIfNeeded();
    extension_host_ = embedder_host_tester->AppendChild("extension_host");
  }

#if BUILDFLAG(ENABLE_PDF_SAVE_TO_DRIVE)
  SaveToDriveFlow* CreateSaveToDriveFlow(
      content::RenderFrameHost* render_frame_host,
      std::unique_ptr<save_to_drive::SaveToDriveEventDispatcher>
          event_dispatcher,
      std::unique_ptr<save_to_drive::ContentReader> content_reader,
      std::unique_ptr<save_to_drive::AccountChooser> account_chooser,
      HatsService* hats_service) {
    if (flow_) {
      ADD_FAILURE() << "SaveToDriveFlow already exists";
      return nullptr;
    }
    flow_ = std::make_unique<MockSaveToDriveFlow>(
        main_rfh(), std::move(event_dispatcher), std::move(content_reader),
        std::move(account_chooser), hats_service);
    return flow_.get();
  }
#endif  // BUILDFLAG(ENABLE_PDF_SAVE_TO_DRIVE)

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
#if BUILDFLAG(ENABLE_PDF_SAVE_TO_DRIVE)
  raw_ptr<extensions::TestEventRouter> event_router_ = nullptr;
  SaveToDriveFlow::CreateCallback create_save_to_drive_flow_callback_;
  std::unique_ptr<MockSaveToDriveFlow> flow_;
  raw_ptr<HatsService> hats_service_ = nullptr;
#endif  // BUILDFLAG(ENABLE_PDF_SAVE_TO_DRIVE)
  raw_ptr<content::RenderFrameHost> extension_host_ = nullptr;
};

// Getting the stream info should fail if there isn't an embedder host.
TEST_F(PdfViewerPrivateApiUnitTest, GetStreamInfoNoEmbedderHost) {
  auto function = base::MakeRefCounted<PdfViewerPrivateGetStreamInfoFunction>();
  function->SetRenderFrameHost(main_rfh());

  EXPECT_EQ("Failed to get StreamContainer",
            api_test_utils::RunFunctionAndReturnError(function.get(), "[]",
                                                      profile()));
}

// Getting the stream info should fail if there isn't an existing stream
// container.
TEST_F(PdfViewerPrivateApiUnitTest, GetStreamInfoNoStreamContainer) {
  auto function = base::MakeRefCounted<PdfViewerPrivateGetStreamInfoFunction>();
  function->SetRenderFrameHost(extension_host());

  EXPECT_EQ("Failed to get StreamContainer",
            api_test_utils::RunFunctionAndReturnError(function.get(), "[]",
                                                      profile()));
}

// Succeed in getting the stream info if there's an existing stream container.
TEST_F(PdfViewerPrivateApiUnitTest, GetStreamInfoValid) {
  constexpr char kExpectedStreamInfo[] = R"({
    "embedded": true,
    "originalUrl": "https://original_url1/",
    "streamUrl": "stream://url1",
    "tabId": 1
  })";

  CreateAndClaimStreamContainer();

  auto function = base::MakeRefCounted<PdfViewerPrivateGetStreamInfoFunction>();
  function->SetRenderFrameHost(extension_host());

  std::optional<base::Value> result =
      api_test_utils::RunFunctionAndReturnSingleResult(function.get(), "[]",
                                                       profile());
  ASSERT_TRUE(result);
  base::Value::Dict* result_dict = result->GetIfDict();
  ASSERT_TRUE(result_dict);

  EXPECT_THAT(*result_dict, base::test::IsJson(kExpectedStreamInfo));
}

// Succeed in setting tab title for a full-page PDF.
TEST_F(PdfViewerPrivateApiUnitTest, SetPdfDocumentTitleFullPagePdf) {
  auto function =
      base::MakeRefCounted<PdfViewerPrivateSetPdfDocumentTitleFunction>();
  function->SetRenderFrameHost(extension_host());

  // Simulate full-page PDF by setting the MIME type of `web_contents()` to
  // "application/pdf".
  content::WebContentsTester::For(web_contents())
      ->SetMainFrameMimeType(pdf::kPDFMimeType);

  ASSERT_TRUE(api_test_utils::RunFunction(function.get(),
                                          "[\"PDF title test\"]", profile()));

  const std::u16string kExpectedTitle = u"PDF title test";
  EXPECT_EQ(kExpectedTitle, web_contents()
                                ->GetController()
                                .GetLastCommittedEntry()
                                ->GetTitleForDisplay());
  EXPECT_EQ(kExpectedTitle, web_contents()->GetTitle());
}

// The following test validates that the DCHECK fails if `setPdfDocumentTitle`
// is not called from a full-page PDF. Outside of debug builds, this will kill
// the renderer rather than crash, hence the test is only run in debug builds.
#ifndef NDEBUG
TEST_F(PdfViewerPrivateApiUnitTest, SetPdfDocumentTitleEmbeddedPdf) {
  auto function =
      base::MakeRefCounted<PdfViewerPrivateSetPdfDocumentTitleFunction>();
  function->SetRenderFrameHost(extension_host());

  // Simulate embedded PDF by setting the MIME type of `web_contents()` to
  // "text/html".
  content::WebContentsTester::For(web_contents())
      ->SetMainFrameMimeType("text/html");

  // Setting title should crash if it's not a full-page PDF.
  ASSERT_DEATH(api_test_utils::RunFunction(function.get(),
                                           "[\"PDF title test\"]", profile()),
               "");
}
#endif

// Getting the stream info should fail if there isn't an embedder host.
TEST_F(PdfViewerPrivateApiUnitTest, SetPdfPluginAttributesNoEmbedderHost) {
  auto function =
      base::MakeRefCounted<PdfViewerPrivateSetPdfPluginAttributesFunction>();
  function->SetRenderFrameHost(main_rfh());

  EXPECT_EQ("Failed to get StreamContainer",
            api_test_utils::RunFunctionAndReturnError(
                function.get(), kSampleSetPdfPluginAttributesArgs, profile()));
}

// Setting PDF plugin attributes should fail if there isn't an existing stream
// container.
TEST_F(PdfViewerPrivateApiUnitTest, SetPdfPluginAttributesNoStreamContainer) {
  auto function =
      base::MakeRefCounted<PdfViewerPrivateSetPdfPluginAttributesFunction>();
  function->SetRenderFrameHost(extension_host());

  EXPECT_EQ("Failed to get StreamContainer",
            api_test_utils::RunFunctionAndReturnError(
                function.get(), kSampleSetPdfPluginAttributesArgs, profile()));
}

// Succeed in setting PDF plugin attributes if there's an existing stream
// container.
TEST_F(PdfViewerPrivateApiUnitTest, SetPdfPluginAttributesValid) {
  CreateAndClaimStreamContainer();

  auto function =
      base::MakeRefCounted<PdfViewerPrivateSetPdfPluginAttributesFunction>();
  function->SetRenderFrameHost(extension_host());

  ASSERT_TRUE(api_test_utils::RunFunction(
      function.get(), kSampleSetPdfPluginAttributesArgs, profile()));

  base::WeakPtr<StreamContainer> stream =
      pdf_viewer_stream_manager()->GetStreamContainer(main_rfh());
  ASSERT_TRUE(stream);
  auto& attributes = stream->pdf_plugin_attributes();
  EXPECT_EQ(attributes->background_color, 10);
  EXPECT_FALSE(attributes->allow_javascript);
}

// Succeed in setting PDF plugin attributes with JavaScript allowed.
TEST_F(PdfViewerPrivateApiUnitTest,
       SetPdfPluginAttributesValidAllowJavaScript) {
  constexpr char kSetPdfPluginAttributesArgs[] = R"([{
    "backgroundColor": 500.0,
    "allowJavascript": true,
  }])";

  CreateAndClaimStreamContainer();

  auto function =
      base::MakeRefCounted<PdfViewerPrivateSetPdfPluginAttributesFunction>();
  function->SetRenderFrameHost(extension_host());

  ASSERT_TRUE(api_test_utils::RunFunction(
      function.get(), kSetPdfPluginAttributesArgs, profile()));

  base::WeakPtr<StreamContainer> stream =
      pdf_viewer_stream_manager()->GetStreamContainer(main_rfh());
  ASSERT_TRUE(stream);
  auto& attributes = stream->pdf_plugin_attributes();
  ASSERT_TRUE(attributes);
  EXPECT_EQ(attributes->background_color, 500);
  EXPECT_TRUE(attributes->allow_javascript);
}

// Fail to set the PDF plugin attributes if the background color is not an
// integer.
TEST_F(PdfViewerPrivateApiUnitTest,
       SetPdfPluginAttributesBackgroundColorInvalidInteger) {
  constexpr char kSetPdfPluginAttributesArgs[] = R"([{
    "backgroundColor": 10.1,
    "allowJavascript": false,
  }])";

  CreateAndClaimStreamContainer();

  auto function =
      base::MakeRefCounted<PdfViewerPrivateSetPdfPluginAttributesFunction>();
  function->SetRenderFrameHost(extension_host());

  EXPECT_EQ("Background color is not an integer",
            api_test_utils::RunFunctionAndReturnError(
                function.get(), kSetPdfPluginAttributesArgs, profile()));
}

// Fail to set the PDF plugin attributes if the background color is negative.
TEST_F(PdfViewerPrivateApiUnitTest,
       SetPdfPluginAttributesBackgroundColorNegative) {
  constexpr char kSetPdfPluginAttributesArgs[] = R"([{
    "backgroundColor": -10.0,
    "allowJavascript": false,
  }])";

  CreateAndClaimStreamContainer();

  auto function =
      base::MakeRefCounted<PdfViewerPrivateSetPdfPluginAttributesFunction>();
  function->SetRenderFrameHost(extension_host());

  EXPECT_EQ("Background color out of bounds",
            api_test_utils::RunFunctionAndReturnError(
                function.get(), kSetPdfPluginAttributesArgs, profile()));
}

// Fail to set the PDF plugin attributes if the background color isn't within
// range of a uint32_t.
TEST_F(PdfViewerPrivateApiUnitTest,
       SetPdfPluginAttributesBackgroundColorUpperBound) {
  constexpr char kSetPdfPluginAttributesArgs[] = R"([{
    "backgroundColor": 4294967296.0,
    "allowJavascript": false,
  }])";

  CreateAndClaimStreamContainer();

  auto function =
      base::MakeRefCounted<PdfViewerPrivateSetPdfPluginAttributesFunction>();
  function->SetRenderFrameHost(extension_host());

  EXPECT_EQ("Background color out of bounds",
            api_test_utils::RunFunctionAndReturnError(
                function.get(), kSetPdfPluginAttributesArgs, profile()));
}

#if BUILDFLAG(ENABLE_PDF_SAVE_TO_DRIVE)
// Succeed in sending a request to save a PDF to Drive.
TEST_F(PdfViewerPrivateApiUnitTest, SaveToDrive) {
  CreateAndClaimStreamContainer();
  auto function = base::MakeRefCounted<PdfViewerPrivateSaveToDriveFunction>();
  function->SetRenderFrameHost(extension_host());

  EXPECT_TRUE(
      api_test_utils::RunFunction(function, R"(["ORIGINAL"])", profile()));
}

// Failed in sending a request to save a PDF to Drive if there is already a
// request in progress.
TEST_F(PdfViewerPrivateApiUnitTest, SaveToDriveFailedIfAlreadyInProgress) {
  CreateAndClaimStreamContainer();
  {
    // Create a new flow to simulate the previous request is in progress.
    SaveToDriveFlow::CreateForCurrentDocument(extension_host(), nullptr,
                                              nullptr, nullptr, hats_service());
    auto* flow = SaveToDriveFlow::GetForCurrentDocument(extension_host());
    ASSERT_TRUE(flow);
  }
  {
    auto function = base::MakeRefCounted<PdfViewerPrivateSaveToDriveFunction>();
    function->SetRenderFrameHost(extension_host());

    EXPECT_EQ("An upload is already in progress",
              api_test_utils::RunFunctionAndReturnError(
                  function.get(), R"(["ORIGINAL"])", profile()));
  }
}

// Succeed in sending a request to save a PDF to Drive after the previous
// request is canceled.
TEST_F(PdfViewerPrivateApiUnitTest, SaveToDriveCanceledAndStartNew) {
  CreateAndClaimStreamContainer();
  {
    // Create a new flow to simulate the previous request is canceled.
    SaveToDriveFlow::CreateForCurrentDocument(extension_host(), nullptr,
                                              nullptr, nullptr, hats_service());
    auto* flow = SaveToDriveFlow::GetForCurrentDocument(extension_host());
    ASSERT_TRUE(flow);
    flow->Stop();
  }
  {
    auto function = base::MakeRefCounted<PdfViewerPrivateSaveToDriveFunction>();
    function->SetRenderFrameHost(extension_host());

    EXPECT_TRUE(api_test_utils::RunFunction(function.get(), R"(["ORIGINAL"])",
                                            profile()));
  }
}

// The request to save a PDF to Drive should fail if the event dispatcher
// fails to be created.
TEST_F(PdfViewerPrivateApiUnitTest, SaveToDriveFailedIfEventDispatcherNull) {
  // Don't create stream container so that event dispatcher fails to be created.
  auto function = base::MakeRefCounted<PdfViewerPrivateSaveToDriveFunction>();
  function->SetRenderFrameHost(extension_host());

  EXPECT_EQ("Failed to create event dispatcher",
            api_test_utils::RunFunctionAndReturnError(
                function.get(), R"(["ORIGINAL"])", profile()));
}

// Succeed in sending a request to cancel the previous request to save a PDF
// to Drive.
TEST_F(PdfViewerPrivateApiUnitTest, SaveToDriveCancelUpload) {
  CreateAndClaimStreamContainer();
  {
    // Create a new flow to simulate the previous request is in progress.
    SaveToDriveFlow::CreateForCurrentDocument(extension_host(), nullptr,
                                              nullptr, nullptr, hats_service());
    auto* flow = SaveToDriveFlow::GetForCurrentDocument(extension_host());
    ASSERT_TRUE(flow);
  }
  {
    auto function = base::MakeRefCounted<PdfViewerPrivateSaveToDriveFunction>();
    function->SetRenderFrameHost(extension_host());
    EXPECT_TRUE(
        api_test_utils::RunFunction(function.get(), R"([])", profile()));
  }
}

// Succeed in sending a request to cancel the previous request to save a PDF
// to Drive.
TEST_F(PdfViewerPrivateApiUnitTest, SaveToDriveCancelUploadFailedIfNoUpload) {
  CreateAndClaimStreamContainer();
  auto function = base::MakeRefCounted<PdfViewerPrivateSaveToDriveFunction>();
  function->SetRenderFrameHost(extension_host());
  EXPECT_EQ("Failed to get SaveToDriveFlow",
            api_test_utils::RunFunctionAndReturnError(function.get(), R"([])",
                                                      profile()));
}

#endif  // BUILDFLAG(ENABLE_PDF_SAVE_TO_DRIVE)

}  // namespace extensions
