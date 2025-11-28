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
      snapshot_(cc::slim::UIResourceLayer::Create()),
      hairline_(cc::slim::SolidColorLayer::Create()) {
  layer()->SetIsDrawable(true);
  container_->SetIsDrawable(true);
  snapshot_->SetIsDrawable(true);
  hairline_->SetIsDrawable(true);

  container_->SetMasksToBounds(true);
  container_->AddChild(snapshot_);
  container_->AddChild(hairline_);
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

// The bookmark bar view is constructed in this way by Android widgets:
//
//         <--- a --->
//         ========================================================
//         |        |
//         |        f
//         |        |
//         |     ==================================================
//       ^ |     | |  <--- c --->
//       | |--e--| d  [FavIcon] Bookmark Name   [FavIcon] Name2 ...
//       | |     | |
//       b |     ==================================================
//       | |
//         |-------------g-----------------------------------------
//         ========================================================
//
// The view contains content, with width/height of (a,b), and a hairline footer
// (g). Within the content, there is a snapshot that is tightly bound (to reduce
// memory impact) around the icons, names, divider, etc of the bookmark bar;
// this has a width/height of (c,d). Since this is tightly bound, we offset it
// within the scene layer by (e,f) to account for padding. The hairline footer
// must appear at the bottom, below the content, which has a height of (b), so
// it needs to be offset by that amount.
//
// The |container_| must encompass all of this content, and so it has two
// children, the |snapshot_| and the |hairline_|, which are a UIResourceLayer to
// hold Bitmap data, and a SolidColorLayer respectively. Its height must include
// both of these. (Not labeled above is that the |container_| is offset by a
// height, which relates to the scrolled amount of the top controls).
//
// We refer to the |container_| as the "scene layer", and the tightly bound
// Bitmap data as the "snapshot". Putting this all together, we have:
//
// |container_|, position relative to its parent (layer()):
//      position = (0, scene_layer_offset_height)
//      width = scene_layer_width = (a)
//      height = scene_layer_height + hairline_height = (b + g)
//
// |snapshot_|, position relative to its parent (|container_|):
//      position = (snapshot_offset_width, snapshot_offset_height) = (e, f)
//      width/height = defined by the Bitmap resource = (c, d)
//
// |hairline_|, position relative to its parent (|container_|):
//      position = (0, scene_layer_height) = (0, b)
//      width = scene_layer_width = (a)
//      height = hairline_height = (g)
//
// For any questions reach out to Bookmark Bar OWNERS.
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
    jint hairline_height,
    jint hairline_background_color,
    const base::android::JavaParamRef<jobject>& joffset_tag) {
  ui::ResourceManager* resource_manager =
      ui::ResourceManagerImpl::FromJavaObject(jresource_manager);
  if (!resource_manager) {
    return;
  }

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
  container_->SetBounds(
      gfx::Size(scene_layer_width, scene_layer_height + hairline_height));
  container_->SetPosition(gfx::PointF(0, scene_layer_offset_height));

  snapshot_->SetUIResourceId(resource->ui_resource()->id());
  snapshot_->SetBounds(resource->size());
  snapshot_->SetPosition(
      gfx::PointF(snapshot_offset_width, snapshot_offset_height));

  hairline_->SetBackgroundColor(
      SkColor4f::FromColor(hairline_background_color));
  hairline_->SetBounds(gfx::Size(scene_layer_width, hairline_height));
  hairline_->SetPosition(gfx::PointF(0, scene_layer_height));

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

DEFINE_JNI(BookmarkBarSceneLayer)
