// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/layer/content_layer.h"

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "cc/slim/layer.h"
#include "cc/slim/ui_resource_layer.h"
#include "chrome/browser/android/compositor/layer/thumbnail_layer.h"
#include "chrome/browser/android/compositor/tab_content_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace android {
namespace {

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

  scoped_refptr<cc::slim::Layer> GetLiveLayer(int tab_id) override {
    return live_layer_;
  }

  ThumbnailLayer* GetStaticLayer(int tab_id) override {
    ++static_layer_lookups_;
    return static_layer_.get();
  }

  void set_live_layer(scoped_refptr<cc::slim::Layer> layer) {
    live_layer_ = std::move(layer);
  }

  ThumbnailLayer* static_layer() { return static_layer_.get(); }
  int static_layer_lookups() const { return static_layer_lookups_; }

 private:
  scoped_refptr<cc::slim::Layer> live_layer_;
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

  content_layer->SetProperties(/*id=*/1, /*can_use_live_layer=*/false,
                               /*can_use_thumbnail=*/false);

  EXPECT_EQ(tab_content_manager_.static_layer_lookups(), 0);
  EXPECT_TRUE(content_layer->layer()->children().empty());
}

TEST_F(ContentLayerTest, CanUseThumbnailTrueAttachesStaticLayer) {
  scoped_refptr<ContentLayer> content_layer =
      ContentLayer::Create(&tab_content_manager_);

  content_layer->SetProperties(/*id=*/1, /*can_use_live_layer=*/false,
                               /*can_use_thumbnail=*/true);

  EXPECT_EQ(tab_content_manager_.static_layer_lookups(), 1);
  ASSERT_EQ(content_layer->layer()->children().size(), 1u);
  EXPECT_EQ(content_layer->layer()->children()[0].get(),
            tab_content_manager_.static_layer()->layer().get());
}

}  // namespace
}  // namespace android
