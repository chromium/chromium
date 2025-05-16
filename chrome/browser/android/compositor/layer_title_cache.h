// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_TITLE_CACHE_H_
#define CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_TITLE_CACHE_H_

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/containers/id_map.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "cc/resources/ui_resource_client.h"
#include "components/tab_groups/tab_group_id.h"

namespace cc::slim {
class Layer;
}

namespace ui {
class ResourceManager;
}

namespace android {

class DecorationTabTitle;
class DecorationIconTitle;

// A native component of the Java LayerTitleCache class.  This class
// will build and maintain layers that represent the cached titles in
// the Java class.
class LayerTitleCache {
 public:
  static LayerTitleCache* FromJavaObject(
      const base::android::JavaRef<jobject>& jobj);

  LayerTitleCache(JNIEnv* env,
                  const jni_zero::JavaRef<jobject>& obj,
                  jint fade_width,
                  jint icon_start_padding,
                  jint icon_end_padding,
                  jint spinner_resource_id,
                  jint spinner_incognito_resource_id,
                  jint bubble_inner_dimension,
                  jint bubble_outer_dimension,
                  jint bubble_offset,
                  jint bubble_inner_tint,
                  jint bubble_outer_tint,
                  ui::ResourceManager* resource_manager);

  LayerTitleCache(const LayerTitleCache&) = delete;
  LayerTitleCache& operator=(const LayerTitleCache&) = delete;

  void Destroy(JNIEnv* env);

  // Called from Java, updates a native cc::slim::Layer based on the new texture
  // information.
  void UpdateLayer(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& obj,
                   jint tab_id,
                   jint title_resource_id,
                   jint icon_resource_id,
                   bool is_incognito,
                   bool is_rtl,
                   bool show_bubble);

  // Called from Java, updates a native cc::slim::Layer based on the new texture
  // information.
  void UpdateGroupLayer(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& obj,
                        const base::android::JavaParamRef<jobject>& group_token,
                        jint title_resource_id,
                        jint avatar_resource_id,
                        jint avatar_padding,
                        bool is_incognito,
                        bool is_rtl);

  // Called from Java, updates icon.
  void UpdateIcon(JNIEnv* env,
                  const base::android::JavaParamRef<jobject>& obj,
                  jint tab_id,
                  jint icon_resource_id,
                  bool show_bubble);

  // Called from Java, updates tab bubble if a shared tab is updated by
  // collaborators.
  void UpdateTabBubble(JNIEnv* env,
                       const base::android::JavaParamRef<jobject>& obj,
                       jint tab_id,
                       bool show_bubble);

  // Returns the layer that represents the title of tab of tab_id.
  // Returns NULL if no layer can be found.
  DecorationTabTitle* GetTitleLayer(int tab_id);

  // Returns the layer that represents the title of group of group_token.
  // Returns NULL if no layer can be found.
  DecorationIconTitle* GetGroupTitleLayer(
      const tab_groups::TabGroupId& group_token,
      bool incognito);

 private:
  const int kEmptyWidth = 0;

  virtual ~LayerTitleCache();

  base::IDMap<std::unique_ptr<DecorationTabTitle>> layer_cache_;
  std::unordered_map<tab_groups::TabGroupId,
                     std::unique_ptr<DecorationIconTitle>,
                     tab_groups::TabGroupIdHash>
      group_layer_cache_;

  JavaObjectWeakGlobalRef weak_java_title_cache_;
  int fade_width_;
  int icon_start_padding_;
  int icon_end_padding_;

  int spinner_resource_id_;
  int spinner_incognito_resource_id_;

  int bubble_inner_dimension_;
  int bubble_outer_dimension_;
  int bubble_offset_;
  int bubble_inner_tint_;
  int bubble_outer_tint_;

  raw_ptr<ui::ResourceManager> resource_manager_;
};

}  // namespace android

#endif  // CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_TITLE_CACHE_H_
