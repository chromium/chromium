// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/scene_layer/bookmark_bar_scene_layer.h"

#include "base/memory/scoped_refptr.h"
#include "cc/input/android/offset_tag_android.h"
#include "cc/slim/layer.h"
#include "cc/slim/solid_color_layer.h"
#include "cc/slim/ui_resource_layer.h"
#include "chrome/browser/ui/android/layouts/scene_layer.h"
#include "components/viz/common/quads/offset_tag.h"
#include "third_party/skia/include/core/SkColor.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/bookmarks/android/jni_headers/BookmarkBarSceneLayer_jni.h"

using base::android::JavaParamRef;

namespace android {

BookmarkBarSceneLayer::BookmarkBarSceneLayer(JNIEnv* env,
                                             const JavaParamRef<jobject>& jobj)
    : SceneLayer(env, jobj),
      container_(cc::slim::SolidColorLayer::Create()),
      snapshot_(cc::slim::UIResourceLayer::Create()) {
  layer()->SetIsDrawable(true);
  container_->SetIsDrawable(true);
  snapshot_->SetIsDrawable(true);

  container_->SetMasksToBounds(true);
  container_->AddChild(snapshot_);
}

BookmarkBarSceneLayer::~BookmarkBarSceneLayer() = default;

void BookmarkBarSceneLayer::SetContentTree(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcontent_tree) {
  SceneLayer* content_tree = FromJavaObject(env, jcontent_tree);
  if (!content_tree || !content_tree->layer()) {
    return;
  }
  if (!content_tree->layer()->parent() ||
      (content_tree->layer()->parent()->id() != layer()->id())) {
    layer()->AddChild(content_tree->layer());
    layer()->AddChild(container_);
  }

  background_color_ = content_tree->GetBackgroundColor();
}

void BookmarkBarSceneLayer::UpdateBookmarkBarLayer(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jresource_manager,
    jint view_resource_id,
    jint scene_layer_background_color,
    jint scene_layer_offset_height,
    jint scene_layer_width,
    jint scene_layer_height,
    jint snapshot_offset_width,
    jint snapshot_offset_height,
    const base::android::JavaParamRef<jobject>& joffset_tag) {
  ui::ResourceManager* resource_manager =
      ui::ResourceManagerImpl::FromJavaObject(jresource_manager);
  ui::Resource* resource = resource_manager->GetResource(
      ui::ANDROID_RESOURCE_TYPE_DYNAMIC_BITMAP, view_resource_id);

  if (!resource) {
    return;
  }

  viz::OffsetTag offset_tag = cc::android::FromJavaOffsetTag(env, joffset_tag);

  // An update to the bookmark bar could come from changing the window size,
  // editing the bookmarks, changing theme, or scrolling. We update all this
  // information on each update.
  container_->SetBackgroundColor(
      SkColor4f::FromColor(scene_layer_background_color));
  container_->SetBounds(gfx::Size(scene_layer_width, scene_layer_height));
  container_->SetPosition(gfx::PointF(0, scene_layer_offset_height));

  snapshot_->SetUIResourceId(resource->ui_resource()->id());
  snapshot_->SetBounds(resource->size());
  snapshot_->SetPosition(
      gfx::PointF(snapshot_offset_width, snapshot_offset_height));

  container_->SetOffsetTag(offset_tag);
}

void BookmarkBarSceneLayer::ShowBookmarkBar(JNIEnv* env) {
  layer()->SetIsDrawable(true);
}

void BookmarkBarSceneLayer::HideBookmarkBar(JNIEnv* env) {
  layer()->SetIsDrawable(false);
}

// SceneLayer implementation.

bool BookmarkBarSceneLayer::ShouldShowBackground() {
  return true;
}

SkColor BookmarkBarSceneLayer::GetBackgroundColor() {
  return background_color_;
}

static jlong JNI_BookmarkBarSceneLayer_Init(JNIEnv* env,
                                            const JavaParamRef<jobject>& jobj) {
  // This will automatically bind to the Java object and pass ownership there.
  BookmarkBarSceneLayer* scene_layer = new BookmarkBarSceneLayer(env, jobj);
  return reinterpret_cast<intptr_t>(scene_layer);
}

}  // namespace android
