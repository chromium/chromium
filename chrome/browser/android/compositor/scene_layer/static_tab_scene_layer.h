// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_STATIC_TAB_SCENE_LAYER_H_
#define CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_STATIC_TAB_SCENE_LAYER_H_

#include <jni.h>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/ui/android/layouts/scene_layer.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"

namespace android {

class ContentLayer;
class TabContentManager;

// A SceneLayer to render a static tab.
class StaticTabSceneLayer : public SceneLayer {
 public:
  StaticTabSceneLayer(JNIEnv* env, const base::android::JavaRef<jobject>& jobj);

  StaticTabSceneLayer(const StaticTabSceneLayer&) = delete;
  StaticTabSceneLayer& operator=(const StaticTabSceneLayer&) = delete;

  ~StaticTabSceneLayer() override;

  bool ShouldShowBackground() override;
  SkColor GetBackgroundColor() override;

  // Update StaticTabSceneLayer with the new parameters.
  void UpdateTabLayer(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& jobj,
                      jint id,
                      jboolean can_use_live_layer,
                      jint default_background_color,
                      jfloat x,
                      jfloat y,
                      jfloat static_to_view_blend,
                      jfloat saturation,
                      const base::android::JavaParamRef<jobject>& joffset_tag);

  void SetTabContentManager(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jobj,
      const base::android::JavaParamRef<jobject>& jtab_content_manager);

 private:
  scoped_refptr<android::ContentLayer> content_layer_;

  raw_ptr<TabContentManager> tab_content_manager_;
  int background_color_;
};

}  // namespace android

#endif  // CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_STATIC_TAB_SCENE_LAYER_H_
