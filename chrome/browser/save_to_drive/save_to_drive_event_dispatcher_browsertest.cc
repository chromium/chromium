// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/save_to_drive/save_to_drive_event_dispatcher.h"

#include "base/test/with_feature_override.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/pdf/pdf_extension_test_base.h"
#include "chrome/browser/pdf/pdf_extension_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/pdf_viewer_private.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/test_event_router_observer.h"
#include "extensions/common/constants.h"
#include "pdf/pdf_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace save_to_drive {

class SaveToDriveEventDispatcherBrowserTest
    : public base::test::WithFeatureOverride,
      public PDFExtensionTestBase {
 public:
  SaveToDriveEventDispatcherBrowserTest()
      : base::test::WithFeatureOverride(chrome_pdf::features::kPdfOopif) {}

  SaveToDriveEventDispatcherBrowserTest(
      const SaveToDriveEventDispatcherBrowserTest&) = delete;
  SaveToDriveEventDispatcherBrowserTest& operator=(
      const SaveToDriveEventDispatcherBrowserTest&) = delete;

  ~SaveToDriveEventDispatcherBrowserTest() override = default;

  bool UseOopif() const override { return GetParam(); }
};

IN_PROC_BROWSER_TEST_P(SaveToDriveEventDispatcherBrowserTest, Notify) {
  GURL page_url =
      ui_test_utils::GetTestUrl(base::FilePath(FILE_PATH_LITERAL("pdf")),
                                base::FilePath(FILE_PATH_LITERAL("test.pdf")));
  auto* extension_frame = LoadPdfGetExtensionHost(page_url);

  ASSERT_TRUE(extension_frame);

  auto dispatcher = SaveToDriveEventDispatcher::Create(extension_frame);
  ASSERT_TRUE(dispatcher);

  namespace pdf_api = extensions::api::pdf_viewer_private;
  pdf_api::SaveToDriveProgress progress;
  progress.status = pdf_api::SaveToDriveStatus::kUploadInProgress;
  progress.uploaded_bytes = 50;

  auto* event_router = extensions::EventRouter::Get(profile());

  extensions::TestEventRouterObserver observer(event_router);

  dispatcher->Notify(progress);

  ASSERT_EQ(observer.events().size(), 1u);
  EXPECT_EQ(observer.events().begin()->first,
            pdf_api::OnSaveToDriveProgress::kEventName);

  extensions::Event* captured_event = observer.events().begin()->second.get();
  ASSERT_TRUE(captured_event);
  EXPECT_EQ(captured_event->event_name,
            pdf_api::OnSaveToDriveProgress::kEventName);

  ASSERT_EQ(captured_event->event_args.size(), 2u);
  // The stream URL is not deterministic, so just check that it's a string and
  // not empty.
  ASSERT_TRUE(captured_event->event_args[0].is_string());
  EXPECT_FALSE(captured_event->event_args[0].GetString().empty());
  EXPECT_EQ(captured_event->event_args[1], progress.ToValue());
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(SaveToDriveEventDispatcherBrowserTest);

}  // namespace save_to_drive
