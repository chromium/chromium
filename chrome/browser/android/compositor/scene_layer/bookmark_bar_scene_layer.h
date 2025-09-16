// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_BOOKMARK_BAR_SCENE_LAYER_H_
#define CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_BOOKMARK_BAR_SCENE_LAYER_H_

#include "base/android/jni_weak_ref.h"
#include "cc/slim/solid_color_layer.h"
#include "cc/slim/ui_resource_layer.h"
#include "chrome/browser/ui/android/layouts/scene_layer.h"
#include "ui/android/resources/resource_manager_impl.h"

namespace android {

class BookmarkBarSceneLayer : public SceneLayer {
 public:
  BookmarkBarSceneLayer(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& jobj);

  BookmarkBarSceneLayer(const BookmarkBarSceneLayer&) = delete;
  BookmarkBarSceneLayer& operator=(const BookmarkBarSceneLayer&) = delete;
  ~BookmarkBarSceneLayer() override;

  void SetContentTree(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcontent_tree);

  void UpdateBookmarkBarLayer(
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
      const base::android::JavaParamRef<jobject>& joffset_tag);

  void ShowBookmarkBar(JNIEnv* env);
  void HideBookmarkBar(JNIEnv* env);

  // SceneLayer implementation.
  bool ShouldShowBackground() override;
  SkColor GetBackgroundColor() override;

 private:
  // The background color that will be provided by the Java-side code so this
  // Layer can be painted with the correct color for the Android-side theme.
  SkColor background_color_;

  // Top level container for the rest of the view. The snapshot will be
  // contained within this container, offset left/top to match the padding of
  // the Android widgets.
  scoped_refptr<cc::slim::SolidColorLayer> container_;

  // The snapshot of the Android widgets. This is a Bitmap / raw pixel data of
  // the Android widgets, tightly bound to the minimum possible size of a
  // snapshot to capture all relevant content.
  scoped_refptr<cc::slim::UIResourceLayer> snapshot_;

  // The solid color hairline at the bottom of the view.
  scoped_refptr<cc::slim::SolidColorLayer> hairline_;
};

}  // namespace android

#endif  // CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_BOOKMARK_BAR_SCENE_LAYER_H_
