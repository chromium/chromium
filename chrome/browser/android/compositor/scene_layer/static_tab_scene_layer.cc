// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/scene_layer/static_tab_scene_layer.h"

#include "cc/input/android/offset_tag_android.h"
#include "cc/slim/filter.h"
#include "cc/slim/layer.h"
#include "chrome/browser/android/compositor/layer/content_layer.h"
#include "chrome/browser/android/compositor/layer_title_cache.h"
#include "chrome/browser/android/compositor/tab_content_manager.h"
#include "chrome/browser/android/tab_android.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/view_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/StaticTabSceneLayer_jni.h"

using base::android::JavaRef;

namespace android {

StaticTabSceneLayer::StaticTabSceneLayer(JNIEnv* env,
                                         const JavaRef<jobject>& jobj,
                                         bool can_use_thumbnail)
    : SceneLayer(env, jobj),
      tab_content_manager_(nullptr),
      can_use_thumbnail_(can_use_thumbnail) {}

StaticTabSceneLayer::~StaticTabSceneLayer() = default;

bool StaticTabSceneLayer::ShouldShowBackground() {
  if (tab_id_ == -1) {
    return false;
  }
  scoped_refptr<cc::slim::Layer> root = layer_->RootLayer();
  return root && root->bounds() != layer_->bounds();
}

SkColor StaticTabSceneLayer::GetBackgroundColor() {
  return background_color_;
}

void StaticTabSceneLayer::UpdateTabLayer(JNIEnv* env,
                                         int32_t id,
                                         bool can_use_live_layer,
                                         int32_t default_background_color,
                                         float x,
                                         float y,
                                         const JavaRef<jobject>& joffset_tag) {
  DCHECK(tab_content_manager_)
      << "TabContentManager must be set before updating the layer";

  tab_id_ = id;
  background_color_ = id == -1 ? SK_ColorTRANSPARENT
                               : static_cast<SkColor>(default_background_color);
  if (!content_layer_.get()) {
    content_layer_ = android::ContentLayer::Create(tab_content_manager_);
    layer_->AddChild(content_layer_->layer());
  }

  if (id != -1 && can_use_live_layer) {
    TabAndroid* tab = tab_content_manager_->GetTab(id);
    if (tab && tab->web_contents() && tab->web_contents()->GetNativeView()) {
      tab->web_contents()->GetNativeView()->set_content_offset_x(x);
    }
  }

  content_layer_->SetProperties(id, can_use_live_layer, can_use_thumbnail_);

  content_layer_->layer()->SetPosition(gfx::PointF(x, y));
  content_layer_->layer()->SetIsDrawable(true);

  viz::OffsetTag tag = cc::android::FromJavaOffsetTag(env, joffset_tag);
  content_layer_->layer()->SetOffsetTag(tag);
}

void StaticTabSceneLayer::SetTabContentManager(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& jtab_content_manager) {
  if (!tab_content_manager_) {
    tab_content_manager_ =
        TabContentManager::FromJavaObject(jtab_content_manager);
  }
}

static int64_t JNI_StaticTabSceneLayer_Init(JNIEnv* env,
                                            const JavaRef<jobject>& jobj,
                                            bool can_use_thumbnail) {
  // This will automatically bind to the Java object and pass ownership there.
  StaticTabSceneLayer* scene_layer =
      new StaticTabSceneLayer(env, jobj, can_use_thumbnail);
  return reinterpret_cast<intptr_t>(scene_layer);
}

}  // namespace android

DEFINE_JNI(StaticTabSceneLayer)
