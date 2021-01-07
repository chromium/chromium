// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base64.h"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/paint_preview/browser/paint_preview_client.h"
#include "components/paint_preview/common/file_stream.h"
#include "components/paint_preview/common/mojom/paint_preview_recorder.mojom-shared.h"
#include "components/paint_preview/common/proto/paint_preview.pb.h"
#include "components/paint_preview/common/recording_map.h"
#include "components/paint_preview/common/serialized_recording.h"
#include "components/paint_preview/common/test_utils.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/notification_types.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkStream.h"
#include "url/gurl.h"

namespace paint_preview {

// Test harness for a integration test of paint previews. In this test:
// - Each RenderFrame has an instance of PaintPreviewRecorder attached.
// - Each WebContents has an instance of PaintPreviewClient attached.
// This permits end-to-end testing of the flow of paint previews.
class PaintPreviewBrowserTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<RecordingPersistence> {
 protected:
  PaintPreviewBrowserTest() = default;
  ~PaintPreviewBrowserTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");

    http_server_.ServeFilesFromSourceDirectory("content/test/data");

    ASSERT_TRUE(http_server_.Start());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    content::IsolateAllSitesForTesting(command_line);
  }

  void CreateClient() {
    PaintPreviewClient::CreateForWebContents(GetWebContents());
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  void LoadPage(const GURL& url) const {
    ui_test_utils::NavigateToURL(browser(), url);
  }

  void LoadHtml(const base::StringPiece& html) const {
    std::string base64_html;
    base::Base64Encode(html, &base64_html);
    GURL url(std::string("data:text/html;base64,") + base64_html);
    ui_test_utils::NavigateToURL(browser(), url);
  }

  PaintPreviewClient::PaintPreviewParams MakeParams() const {
    PaintPreviewClient::PaintPreviewParams params(GetParam());
    params.inner.is_main_frame = true;
    params.root_dir = temp_dir_.GetPath();
    params.inner.capture_links = true;
    params.inner.max_capture_size = 0;
    return params;
  }

  void WaitForLoadStopWithoutSuccessCheck() {
    // In many cases, the load may have finished before we get here.  Only wait
    // if the tab still has a pending navigation.
    auto* web_contents = GetWebContents();
    if (web_contents->IsLoading()) {
      content::WindowedNotificationObserver load_stop_observer(
          content::NOTIFICATION_LOAD_STOP,
          content::Source<content::NavigationController>(
              &web_contents->GetController()));
      load_stop_observer.Wait();
    }
  }

  // Check that |recording_map| contains the frame |frame_proto| and is a valid
  // SkPicture. Don't bother checking the contents as this is non-trivial and
  // could change. Instead check that the SkPicture can be read correctly and
  // has a cull rect of at least |size|.
  //
  // Consumes the recording from |recording_map|.
  static void EnsureSkPictureIsValid(RecordingMap* recording_map,
                                     const PaintPreviewFrameProto& frame_proto,
                                     size_t expected_subframe_count,
                                     const gfx::Size& size = gfx::Size(1, 1)) {
    base::ScopedAllowBlockingForTesting scoped_blocking;

    auto it = recording_map->find(base::UnguessableToken::Deserialize(
        frame_proto.embedding_token_high(), frame_proto.embedding_token_low()));
    ASSERT_NE(it, recording_map->end());

    base::Optional<SkpResult> result = std::move(it->second).Deserialize();
    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result->skp, nullptr);
    EXPECT_GE(result->skp->cullRect().width(), 0);
    EXPECT_GE(result->skp->cullRect().height(), 0);
    EXPECT_EQ(result->ctx.size(), expected_subframe_count);

    recording_map->erase(it);
  }

  base::ScopedTempDir temp_dir_;
  net::EmbeddedTestServer http_server_;
  net::EmbeddedTestServer http_server_different_origin_;

 private:
  PaintPreviewBrowserTest(const PaintPreviewBrowserTest&) = delete;
  PaintPreviewBrowserTest& operator=(const PaintPreviewBrowserTest&) = delete;
};

IN_PROC_BROWSER_TEST_P(PaintPreviewBrowserTest, CaptureFrame) {
  LoadPage(http_server_.GetURL("a.com", "/cross_site_iframe_factory.html?a"));
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  auto params = MakeParams();

  base::RunLoop loop;

  CreateClient();
  auto* client = PaintPreviewClient::FromWebContents(GetWebContents());
  WaitForLoadStopWithoutSuccessCheck();
  client->CapturePaintPreview(
      params, GetWebContents()->GetMainFrame(),
      base::BindOnce(
          [](base::RepeatingClosure quit,
             const PaintPreviewClient::PaintPreviewParams& params,
             base::UnguessableToken guid, mojom::PaintPreviewStatus status,
             std::unique_ptr<CaptureResult> result) {
            EXPECT_EQ(guid, params.inner.document_guid);
            EXPECT_EQ(status, mojom::PaintPreviewStatus::kOk);
            EXPECT_TRUE(result->proto.has_root_frame());
            EXPECT_EQ(result->proto.subframes_size(), 0);
            EXPECT_EQ(result->proto.root_frame()
                          .content_id_to_embedding_tokens_size(),
                      0);
            EXPECT_TRUE(result->proto.root_frame().is_main_frame());
            {
              base::ScopedAllowBlockingForTesting scoped_blocking;
              auto pair = RecordingMapFromCaptureResult(std::move(*result));
              EnsureSkPictureIsValid(&pair.first, pair.second.root_frame(), 0);
            }
            quit.Run();
          },
          loop.QuitClosure(), params));
  loop.Run();

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::PaintPreviewCapture::kEntryName);
  EXPECT_EQ(1u, entries.size());
}

IN_PROC_BROWSER_TEST_P(PaintPreviewBrowserTest,
                       CaptureMainFrameWithCrossProcessSubframe) {
  LoadPage(
      http_server_.GetURL("a.com", "/cross_site_iframe_factory.html?a(b)"));
  auto params = MakeParams();

  base::RunLoop loop;

  CreateClient();
  auto* client = PaintPreviewClient::FromWebContents(GetWebContents());
  WaitForLoadStopWithoutSuccessCheck();
  client->CapturePaintPreview(
      params, GetWebContents()->GetMainFrame(),
      base::BindOnce(
          [](base::RepeatingClosure quit,
             const PaintPreviewClient::PaintPreviewParams& params,
             base::UnguessableToken guid, mojom::PaintPreviewStatus status,
             std::unique_ptr<CaptureResult> result) {
            EXPECT_EQ(guid, params.inner.document_guid);
            EXPECT_EQ(status, mojom::PaintPreviewStatus::kOk);
            EXPECT_TRUE(result->proto.has_root_frame());
            EXPECT_EQ(result->proto.subframes_size(), 1);
            EXPECT_EQ(result->proto.root_frame()
                          .content_id_to_embedding_tokens_size(),
                      1);
            EXPECT_TRUE(result->proto.root_frame().is_main_frame());
            EXPECT_EQ(result->proto.subframes(0)
                          .content_id_to_embedding_tokens_size(),
                      0);
            EXPECT_FALSE(result->proto.subframes(0).is_main_frame());
            {
              base::ScopedAllowBlockingForTesting scoped_blocking;
              auto pair = RecordingMapFromCaptureResult(std::move(*result));
              EnsureSkPictureIsValid(&pair.first, pair.second.root_frame(), 1);
              EnsureSkPictureIsValid(&pair.first, pair.second.subframes(0), 0);
            }
            quit.Run();
          },
          loop.QuitClosure(), params));
  loop.Run();
}

IN_PROC_BROWSER_TEST_P(PaintPreviewBrowserTest,
                       CaptureMainFrameWithScrollableSameProcessSubframe) {
  std::string html = R"(<html>
                          <iframe
                            srcdoc="<div
                                      style='width: 300px;
                                             height: 300px;
                                             background-color: #ff0000'>
                                      &nbsp;
                                    </div>"
                            title="subframe"
                            width="100px"
                            height="100px">
                          </iframe>
                        </html>)";
  LoadHtml(html);
  auto params = MakeParams();

  base::RunLoop loop;
  CreateClient();
  auto* client = PaintPreviewClient::FromWebContents(GetWebContents());
  WaitForLoadStopWithoutSuccessCheck();
  client->CapturePaintPreview(
      params, GetWebContents()->GetMainFrame(),
      base::BindOnce(
          [](base::RepeatingClosure quit,
             const PaintPreviewClient::PaintPreviewParams& params,
             base::UnguessableToken guid, mojom::PaintPreviewStatus status,
             std::unique_ptr<CaptureResult> result) {
            EXPECT_EQ(guid, params.inner.document_guid);
            EXPECT_EQ(status, mojom::PaintPreviewStatus::kOk);
            EXPECT_TRUE(result->proto.has_root_frame());
            EXPECT_EQ(result->proto.subframes_size(), 1);
            EXPECT_EQ(result->proto.root_frame()
                          .content_id_to_embedding_tokens_size(),
                      1);
            EXPECT_TRUE(result->proto.root_frame().is_main_frame());
            EXPECT_EQ(result->proto.subframes(0)
                          .content_id_to_embedding_tokens_size(),
                      0);
            EXPECT_FALSE(result->proto.subframes(0).is_main_frame());
            {
              base::ScopedAllowBlockingForTesting scoped_blocking;
              auto pair = RecordingMapFromCaptureResult(std::move(*result));
              EnsureSkPictureIsValid(&pair.first, pair.second.root_frame(), 1);
              EnsureSkPictureIsValid(&pair.first, pair.second.subframes(0), 0,
                                     gfx::Size(300, 300));
            }
            quit.Run();
          },
          loop.QuitClosure(), params));
  loop.Run();
}

IN_PROC_BROWSER_TEST_P(PaintPreviewBrowserTest,
                       CaptureMainFrameWithNonScrollableSameProcessSubframe) {
  std::string html = R"(<html>
                          <iframe
                            srcdoc="<div
                                      style='width: 50px;
                                             height: 50px;
                                             background-color: #ff0000'>
                                      &nbsp;
                                    </div>"
                            title="subframe"
                            width="100px"
                            height="100px">
                          </iframe>
                        </html>)";
  LoadHtml(html);
  auto params = MakeParams();

  base::RunLoop loop;
  CreateClient();
  auto* client = PaintPreviewClient::FromWebContents(GetWebContents());
  WaitForLoadStopWithoutSuccessCheck();
  client->CapturePaintPreview(
      params, GetWebContents()->GetMainFrame(),
      base::BindOnce(
          [](base::RepeatingClosure quit,
             const PaintPreviewClient::PaintPreviewParams& params,
             base::UnguessableToken guid, mojom::PaintPreviewStatus status,
             std::unique_ptr<CaptureResult> result) {
            EXPECT_EQ(guid, params.inner.document_guid);
            EXPECT_EQ(status, mojom::PaintPreviewStatus::kOk);
            EXPECT_TRUE(result->proto.has_root_frame());
            EXPECT_EQ(result->proto.subframes_size(), 0);
            EXPECT_EQ(result->proto.root_frame()
                          .content_id_to_embedding_tokens_size(),
                      0);
            EXPECT_TRUE(result->proto.root_frame().is_main_frame());
            {
              base::ScopedAllowBlockingForTesting scoped_blocking;
              auto pair = RecordingMapFromCaptureResult(std::move(*result));
              EnsureSkPictureIsValid(&pair.first, pair.second.root_frame(), 0);
            }
            quit.Run();
          },
          loop.QuitClosure(), params));
  loop.Run();
}

INSTANTIATE_TEST_SUITE_P(All,
                         PaintPreviewBrowserTest,
                         testing::Values(RecordingPersistence::kFileSystem,
                                         RecordingPersistence::kMemoryBuffer),
                         PersistenceParamToString);

}  // namespace paint_preview
