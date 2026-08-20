// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/widget/glic_side_panel_ui_android.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/memory/raw_ref.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/service/glic_ui_embedder.h"
#include "chrome/browser/glic/service/metrics/glic_instance_metrics.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/metrics/profile_metrics_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/drop_data.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/page/drag_operation.h"
#include "url/gurl.h"

namespace glic {
namespace {

class FakeGlicUiEmbedderDelegate : public GlicUiEmbedder::Delegate {
 public:
  explicit FakeGlicUiEmbedderDelegate(Host& host) : host_(host) {}
  ~FakeGlicUiEmbedderDelegate() override = default;

  void OnEmbedderWindowActivationChanged(bool has_focus) override {}
  void SwitchConversation(
      const ShowOptions& options,
      glic::mojom::ConversationInfoPtr info,
      mojom::WebClientHandler::SwitchConversationCallback callback) override {}
  void DidCloseFor(EmbedderKey key, EmbedderCloseReason reason) override {}
  Host& host() override { return *host_; }
  void Show(ShowOptions options) override {}
  void Detach(tabs::TabInterface& tab) override {}
  void Attach(tabs::TabHandle tab) override {}
  void NotifyPanelStateChanged() override {}

 private:
  raw_ref<Host> host_;
};

}  // namespace

class GlicSidePanelUiAndroidTest : public testing::Test {
 public:
  GlicSidePanelUiAndroidTest()
      : host_(&profile_, nullptr, nullptr, nullptr),
        delegate_(host_),
        instance_metrics_(&profile_metrics_service_, &profile_) {}
  ~GlicSidePanelUiAndroidTest() override = default;

  TestingProfile* profile() { return &profile_; }
  GlicUiEmbedder::Delegate& delegate() { return delegate_; }
  GlicInstanceMetrics& instance_metrics() { return instance_metrics_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  metrics::ProfileMetricsService profile_metrics_service_;
  Host host_;
  FakeGlicUiEmbedderDelegate delegate_;
  GlicInstanceMetrics instance_metrics_;
};

TEST_F(GlicSidePanelUiAndroidTest, CanDragEnter) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({features::kGlicDragAndDropFileUpload,
                                 features::kGlicDragAndDropFileUploadAndroid},
                                {});

  GlicSidePanelUi side_panel_ui(profile(), base::WeakPtr<tabs::TabInterface>(),
                                delegate(), instance_metrics());

  content::DropData drop_data;
  blink::DragOperationsMask ops = blink::kDragOperationCopy;

  // Empty DropData should be rejected.
  EXPECT_FALSE(side_panel_ui.CanDragEnter(nullptr, drop_data, ops));

  // DropData with files should be accepted.
  drop_data.filenames.emplace_back(
      base::FilePath(FILE_PATH_LITERAL("test.txt")),
      base::FilePath(FILE_PATH_LITERAL("test.txt")));
  EXPECT_TRUE(side_panel_ui.CanDragEnter(nullptr, drop_data, ops));

  // DropData with file_system_files should be accepted.
  drop_data.filenames.clear();
  drop_data.file_system_files.emplace_back(
      GURL("filesystem:http://example.com/test.txt"), 100, "test.txt");
  EXPECT_TRUE(side_panel_ui.CanDragEnter(nullptr, drop_data, ops));

  // DropData with URL should be rejected (OS file drops only on Android).
  drop_data.file_system_files.clear();
  drop_data.url_infos.emplace_back(GURL("https://example.com"),
                                   std::u16string());
  EXPECT_FALSE(side_panel_ui.CanDragEnter(nullptr, drop_data, ops));

  // DropData with text should be rejected.
  drop_data.url_infos.clear();
  drop_data.text = u"test text";
  EXPECT_FALSE(side_panel_ui.CanDragEnter(nullptr, drop_data, ops));

  // DropData with html should be rejected.
  drop_data.text.reset();
  drop_data.html = u"<b>test html</b>";
  EXPECT_FALSE(side_panel_ui.CanDragEnter(nullptr, drop_data, ops));
}

TEST_F(GlicSidePanelUiAndroidTest, CanDragEnter_FeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kGlicDragAndDropFileUpload);

  GlicSidePanelUi side_panel_ui(profile(), base::WeakPtr<tabs::TabInterface>(),
                                delegate(), instance_metrics());

  content::DropData drop_data;
  drop_data.filenames.emplace_back(
      base::FilePath(FILE_PATH_LITERAL("test.txt")),
      base::FilePath(FILE_PATH_LITERAL("test.txt")));
  blink::DragOperationsMask ops = blink::kDragOperationCopy;

  // Should be rejected because kGlicDragAndDropFileUpload is disabled.
  EXPECT_FALSE(side_panel_ui.CanDragEnter(nullptr, drop_data, ops));
}

TEST_F(GlicSidePanelUiAndroidTest, CanDragEnter_AndroidKillSwitchDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kGlicDragAndDropFileUpload},
      /*disabled_features=*/{features::kGlicDragAndDropFileUploadAndroid});

  GlicSidePanelUi side_panel_ui(profile(), base::WeakPtr<tabs::TabInterface>(),
                                delegate(), instance_metrics());

  content::DropData drop_data;
  drop_data.filenames.emplace_back(
      base::FilePath(FILE_PATH_LITERAL("test.txt")),
      base::FilePath(FILE_PATH_LITERAL("test.txt")));
  blink::DragOperationsMask ops = blink::kDragOperationCopy;

  // Should be rejected because kGlicDragAndDropFileUploadAndroid is disabled.
  EXPECT_FALSE(side_panel_ui.CanDragEnter(nullptr, drop_data, ops));
}

}  // namespace glic
