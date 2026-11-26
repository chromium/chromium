// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/layer/content_layer.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "cc/slim/layer.h"
#include "cc/slim/layer_tree.h"
#include "cc/slim/layer_tree_client.h"
#include "chrome/browser/android/compositor/layer/thumbnail_layer.h"
#include "chrome/browser/android/compositor/tab_content_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace android {
namespace {

class StubLayerTreeClient : public cc::slim::LayerTreeClient {
 public:
  void BeginFrame(const viz::BeginFrameArgs& args) override {}
  void DidSubmitCompositorFrame() override {}
  void DidReceiveCompositorFrameAck() override {}
  void RequestNewFrameSink() override {}
  void DidInitializeLayerTreeFrameSink() override {}
  void DidFailToInitializeLayerTreeFrameSink() override {}
  void DidLoseLayerTreeFrameSink() override {}
};

class RecordingLayer : public cc::slim::Layer {
 public:
  static scoped_refptr<RecordingLayer> Create() {
    return base::AdoptRef(new RecordingLayer());
  }

  void SetLayerTree(cc::slim::LayerTree* layer_tree) override {
    ++set_layer_tree_calls_;
    cc::slim::Layer::SetLayerTree(layer_tree);
  }

  int set_layer_tree_calls() const { return set_layer_tree_calls_; }

 private:
  RecordingLayer() = default;
  ~RecordingLayer() override = default;

  int set_layer_tree_calls_ = 0;
};

class FakeTabContentManager : public TabContentManager {
 public:
  FakeTabContentManager()
      : TabContentManager(base::android::AttachCurrentThread(),
                          base::android::JavaRef<jobject>(),
                          /*default_cache_size=*/1,
                          /*compression_queue_max_size=*/1,
                          /*write_queue_max_size=*/1,
                          /*save_jpeg_thumbnails=*/false),
        static_layer_(ThumbnailLayer::Create()) {}

  ThumbnailLayer* GetStaticLayer(int tab_id) override {
    ++static_layer_lookups_;
    return static_layer_.get();
  }

  ThumbnailLayer* static_layer() { return static_layer_.get(); }
  int static_layer_lookups() const { return static_layer_lookups_; }

 private:
  scoped_refptr<ThumbnailLayer> static_layer_;
  int static_layer_lookups_ = 0;
};

class ContentLayerTest : public testing::Test {
 protected:
  content::BrowserTaskEnvironment task_environment_;
  FakeTabContentManager tab_content_manager_;
};

TEST_F(ContentLayerTest, CanUseThumbnailFalseSkipsStaticLayer) {
  scoped_refptr<ContentLayer> content_layer =
      ContentLayer::Create(&tab_content_manager_);

  content_layer->SetProperties(/*id=*/1, /*can_use_thumbnail=*/false,
                               /*live_layer=*/nullptr);

  EXPECT_EQ(tab_content_manager_.static_layer_lookups(), 0);
  EXPECT_TRUE(content_layer->layer()->children().empty());
}

TEST_F(ContentLayerTest, CanUseThumbnailTrueAttachesStaticLayer) {
  scoped_refptr<ContentLayer> content_layer =
      ContentLayer::Create(&tab_content_manager_);

  content_layer->SetProperties(/*id=*/1, /*can_use_thumbnail=*/true,
                               /*live_layer=*/nullptr);

  EXPECT_EQ(tab_content_manager_.static_layer_lookups(), 1);
  ASSERT_EQ(content_layer->layer()->children().size(), 1u);
  EXPECT_EQ(content_layer->layer()->children()[0].get(),
            tab_content_manager_.static_layer()->layer().get());
}

TEST_F(ContentLayerTest, RepeatedSetPropertiesPreservesLiveLayer) {
  StubLayerTreeClient client;
  std::unique_ptr<cc::slim::LayerTree> layer_tree =
      cc::slim::LayerTree::Create(&client);
  scoped_refptr<ContentLayer> content_layer =
      ContentLayer::Create(&tab_content_manager_);
  layer_tree->SetRoot(content_layer->layer());

  scoped_refptr<RecordingLayer> live_layer = RecordingLayer::Create();

  content_layer->SetProperties(/*id=*/1, /*can_use_thumbnail=*/false,
                               live_layer);
  ASSERT_EQ(content_layer->layer()->children().size(), 1u);
  EXPECT_EQ(content_layer->layer()->children()[0].get(), live_layer.get());
  EXPECT_EQ(live_layer->set_layer_tree_calls(), 1);

  // Switching static_layer in and out must keep live_layer attached without
  // detaching/re-attaching it.
  content_layer->SetProperties(/*id=*/1, /*can_use_thumbnail=*/true,
                               live_layer);
  ASSERT_EQ(content_layer->layer()->children().size(), 2u);
  EXPECT_EQ(content_layer->layer()->children()[0].get(), live_layer.get());
  EXPECT_EQ(content_layer->layer()->children()[1].get(),
            tab_content_manager_.static_layer()->layer().get());
  EXPECT_EQ(live_layer->set_layer_tree_calls(), 1);

  content_layer->SetProperties(/*id=*/1, /*can_use_thumbnail=*/false,
                               live_layer);
  ASSERT_EQ(content_layer->layer()->children().size(), 1u);
  EXPECT_EQ(content_layer->layer()->children()[0].get(), live_layer.get());
  EXPECT_EQ(tab_content_manager_.static_layer()->layer()->parent(), nullptr);
  EXPECT_EQ(live_layer->set_layer_tree_calls(), 1);

  // Detaching when `layer_->children()` holds the last reference must not UAF.
  live_layer.reset();
  content_layer->SetProperties(/*id=*/-1, /*can_use_thumbnail=*/false,
                               /*live_layer=*/nullptr);
  EXPECT_TRUE(content_layer->layer()->children().empty());

  // Attaching live_layer while static_layer is already at index 0 must insert
  // live_layer ahead of static_layer.
  content_layer->SetProperties(/*id=*/1, /*can_use_thumbnail=*/true,
                               /*live_layer=*/nullptr);
  ASSERT_EQ(content_layer->layer()->children().size(), 1u);
  EXPECT_EQ(content_layer->layer()->children()[0].get(),
            tab_content_manager_.static_layer()->layer().get());

  scoped_refptr<RecordingLayer> new_live_layer = RecordingLayer::Create();
  content_layer->SetProperties(/*id=*/1, /*can_use_thumbnail=*/true,
                               new_live_layer);
  ASSERT_EQ(content_layer->layer()->children().size(), 2u);
  EXPECT_EQ(content_layer->layer()->children()[0].get(), new_live_layer.get());
  EXPECT_EQ(content_layer->layer()->children()[1].get(),
            tab_content_manager_.static_layer()->layer().get());

  layer_tree->SetRoot(nullptr);
}

}  // namespace
}  // namespace android
