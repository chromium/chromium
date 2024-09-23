// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/layer_title_cache.h"

#include <android/bitmap.h>

#include <memory>

#include "cc/layers/layer.h"
#include "cc/layers/ui_resource_layer.h"
#include "chrome/browser/android/compositor/decoration_title.h"
#include "ui/android/resources/resource_manager.h"
#include "ui/android/resources/resource_manager_impl.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/LayerTitleCache_jni.h"

using base::android::JavaParamRef;
using base::android::JavaRef;

namespace android {

// static
LayerTitleCache* LayerTitleCache::FromJavaObject(const JavaRef<jobject>& jobj) {
  if (jobj.is_null())
    return nullptr;
  return reinterpret_cast<LayerTitleCache*>(Java_LayerTitleCache_getNativePtr(
      base::android::AttachCurrentThread(), jobj));
}

LayerTitleCache::LayerTitleCache(JNIEnv* env,
                                 const jni_zero::JavaRef<jobject>& obj,
                                 jint fade_width,
                                 jint favicon_start_padding,
                                 jint favicon_end_padding,
                                 jint spinner_resource_id,
                                 jint spinner_incognito_resource_id,
                                 ui::ResourceManager* resource_manager)
    : weak_java_title_cache_(env, obj),
      fade_width_(fade_width),
      favicon_start_padding_(favicon_start_padding),
      favicon_end_padding_(favicon_end_padding),
      spinner_resource_id_(spinner_resource_id),
      spinner_incognito_resource_id_(spinner_incognito_resource_id),
      resource_manager_(resource_manager) {}

void LayerTitleCache::Destroy(JNIEnv* env) {
  delete this;
}

void LayerTitleCache::UpdateLayer(JNIEnv* env,
                                  const JavaParamRef<jobject>& obj,
                                  jint tab_id,
                                  jint title_resource_id,
                                  jint favicon_resource_id,
                                  bool is_incognito,
                                  bool is_rtl) {
  DecorationTitle* title_layer = layer_cache_.Lookup(tab_id);
  if (title_layer) {
    if (title_resource_id != -1 && favicon_resource_id != -1) {
      title_layer->Update(title_resource_id, favicon_resource_id, fade_width_,
                          favicon_start_padding_, favicon_end_padding_,
                          is_incognito, is_rtl);
    } else {
      layer_cache_.Remove(tab_id);
    }
  } else {
    layer_cache_.AddWithID(
        std::make_unique<DecorationTitle>(
            resource_manager_, title_resource_id, favicon_resource_id,
            spinner_resource_id_, spinner_incognito_resource_id_, fade_width_,
            favicon_start_padding_, favicon_end_padding_, is_incognito, is_rtl),
        tab_id);
  }
}

void LayerTitleCache::UpdateGroupLayer(JNIEnv* env,
                                       const JavaParamRef<jobject>& obj,
                                       jint group_root_id,
                                       jint title_resource_id,
                                       bool is_incognito,
                                       bool is_rtl) {
  DecorationTitle* title_layer = group_layer_cache_.Lookup(group_root_id);
  if (title_layer) {
    if (title_resource_id != -1) {
      title_layer->Update(title_resource_id, kInvalidResourceId, fade_width_,
                          kEmptyWidth, kEmptyWidth, is_incognito, is_rtl);
    } else {
      group_layer_cache_.Remove(group_root_id);
    }
  } else {
    group_layer_cache_.AddWithID(
        std::make_unique<DecorationTitle>(
            resource_manager_, title_resource_id, kInvalidResourceId,
            kInvalidResourceId, kInvalidResourceId, fade_width_, kEmptyWidth,
            kEmptyWidth, is_incognito, is_rtl),
        group_root_id);
  }
}

void LayerTitleCache::UpdateFavicon(JNIEnv* env,
                                    const JavaParamRef<jobject>& obj,
                                    jint tab_id,
                                    jint favicon_resource_id) {
  DecorationTitle* title_layer = layer_cache_.Lookup(tab_id);
  if (title_layer && favicon_resource_id != -1) {
    title_layer->SetFaviconResourceId(favicon_resource_id);
  }
}

void LayerTitleCache::ClearExcept(JNIEnv* env,
                                  const JavaParamRef<jobject>& obj,
                                  jint except_id) {
  if (except_id == -1) {
    layer_cache_.Clear();
    return;
  }
  base::IDMap<std::unique_ptr<DecorationTitle>>::iterator iter(&layer_cache_);
  for (; !iter.IsAtEnd(); iter.Advance()) {
    const int id = iter.GetCurrentKey();
    if (id != except_id)
      layer_cache_.Remove(id);
  }
}

DecorationTitle* LayerTitleCache::GetTitleLayer(int tab_id) {
  if (!layer_cache_.Lookup(tab_id)) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_LayerTitleCache_buildUpdatedTitle(env, weak_java_title_cache_.get(env),
        tab_id);
  }

  return layer_cache_.Lookup(tab_id);
}

DecorationTitle* LayerTitleCache::GetGroupTitleLayer(int group_root_id,
                                                     bool incognito) {
  if (!group_layer_cache_.Lookup(group_root_id)) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_LayerTitleCache_buildUpdatedGroupTitle(
        env, weak_java_title_cache_.get(env), group_root_id, incognito);
  }

  return group_layer_cache_.Lookup(group_root_id);
}

LayerTitleCache::~LayerTitleCache() = default;

// ----------------------------------------------------------------------------
// Native JNI methods
// ----------------------------------------------------------------------------

jlong JNI_LayerTitleCache_Init(JNIEnv* env,
                               const JavaParamRef<jobject>& obj,
                               jint fade_width,
                               jint favicon_start_padding,
                               jint favicon_end_padding,
                               jint spinner_resource_id,
                               jint spinner_incognito_resource_id,
                               const JavaParamRef<jobject>& jresource_manager) {
  ui::ResourceManager* resource_manager =
      ui::ResourceManagerImpl::FromJavaObject(jresource_manager);
  LayerTitleCache* cache = new LayerTitleCache(
      env, obj, fade_width, favicon_start_padding, favicon_end_padding,
      spinner_resource_id, spinner_incognito_resource_id, resource_manager);
  return reinterpret_cast<intptr_t>(cache);
}

}  // namespace android
