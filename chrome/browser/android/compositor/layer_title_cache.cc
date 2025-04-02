// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/layer_title_cache.h"

#include <android/bitmap.h>

#include <memory>

#include "base/android/token_android.h"
#include "cc/layers/layer.h"
#include "cc/layers/ui_resource_layer.h"
#include "chrome/browser/android/compositor/decoration_icon_title.h"
#include "chrome/browser/android/compositor/decoration_tab_title.h"
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
                                 jint icon_start_padding,
                                 jint icon_end_padding,
                                 jint spinner_resource_id,
                                 jint spinner_incognito_resource_id,
                                 jint bubble_inner_dimension,
                                 jint bubble_outer_dimension,
                                 jint bubble_offset,
                                 jint bubble_inner_tint,
                                 jint bubble_outer_tint,
                                 ui::ResourceManager* resource_manager)
    : weak_java_title_cache_(env, obj),
      fade_width_(fade_width),
      icon_start_padding_(icon_start_padding),
      icon_end_padding_(icon_end_padding),
      spinner_resource_id_(spinner_resource_id),
      spinner_incognito_resource_id_(spinner_incognito_resource_id),
      bubble_inner_dimension_(bubble_inner_dimension),
      bubble_outer_dimension_(bubble_outer_dimension),
      bubble_offset_(bubble_offset),
      bubble_inner_tint_(bubble_inner_tint),
      bubble_outer_tint_(bubble_outer_tint),
      resource_manager_(resource_manager) {}

void LayerTitleCache::Destroy(JNIEnv* env) {
  delete this;
}

void LayerTitleCache::UpdateLayer(JNIEnv* env,
                                  const JavaParamRef<jobject>& obj,
                                  jint tab_id,
                                  jint title_resource_id,
                                  jint icon_resource_id,
                                  bool is_incognito,
                                  bool is_rtl,
                                  bool show_bubble) {
  DecorationTabTitle* title_layer = layer_cache_.Lookup(tab_id);
  if (title_layer) {
    if (title_resource_id != ui::Resource::kInvalidResourceId &&
        icon_resource_id != ui::Resource::kInvalidResourceId) {
      title_layer->Update(title_resource_id, icon_resource_id, fade_width_,
                          icon_start_padding_, icon_end_padding_, is_incognito,
                          is_rtl, show_bubble);
    } else {
      layer_cache_.Remove(tab_id);
    }
  } else {
    layer_cache_.AddWithID(
        std::make_unique<DecorationTabTitle>(
            resource_manager_, title_resource_id, icon_resource_id,
            spinner_resource_id_, spinner_incognito_resource_id_, fade_width_,
            icon_start_padding_, icon_end_padding_, is_incognito, is_rtl,
            show_bubble, bubble_inner_dimension_, bubble_outer_dimension_,
            bubble_offset_, bubble_inner_tint_, bubble_outer_tint_),
        tab_id);
  }
}

void LayerTitleCache::UpdateGroupLayer(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jobject>& jgroup_token,
    jint title_resource_id,
    jint avatar_resource_id,
    jint avatar_padding,
    bool is_incognito,
    bool is_rtl) {
  const tab_groups::TabGroupId& group_token =
      tab_groups::TabGroupId::FromRawToken(
          base::android::TokenAndroid::FromJavaToken(env, jgroup_token));
  auto it = group_layer_cache_.find(group_token);
  if (it != group_layer_cache_.end()) {
    DecorationIconTitle* title_layer = it->second.get();
    if (title_resource_id != ui::Resource::kInvalidResourceId) {
      title_layer->Update(title_resource_id, avatar_resource_id, fade_width_,
                          kEmptyWidth, avatar_padding, is_incognito, is_rtl);
    } else {
      group_layer_cache_.erase(it);
    }
  } else {
    group_layer_cache_.emplace(
        group_token,
        std::make_unique<DecorationIconTitle>(
            resource_manager_, title_resource_id, avatar_resource_id,
            fade_width_, kEmptyWidth, avatar_padding, is_incognito, is_rtl));
  }
}

void LayerTitleCache::UpdateIcon(JNIEnv* env,
                                 const JavaParamRef<jobject>& obj,
                                 jint tab_id,
                                 jint icon_resource_id,
                                 bool show_bubble) {
  DecorationTabTitle* title_layer = layer_cache_.Lookup(tab_id);
  if (title_layer && icon_resource_id != ui::Resource::kInvalidResourceId) {
    title_layer->SetIconResourceId(icon_resource_id);
  }
}

void LayerTitleCache::UpdateTabBubble(JNIEnv* env,
                                      const JavaParamRef<jobject>& obj,
                                      jint tab_id,
                                      bool show_bubble) {
  DecorationTabTitle* title_layer = layer_cache_.Lookup(tab_id);
  if (title_layer) {
    title_layer->SetShowBubble(show_bubble);
  }
}

DecorationTabTitle* LayerTitleCache::GetTitleLayer(int tab_id) {
  if (!layer_cache_.Lookup(tab_id)) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_LayerTitleCache_buildUpdatedTitle(env, weak_java_title_cache_.get(env),
        tab_id);
  }

  return layer_cache_.Lookup(tab_id);
}

DecorationIconTitle* LayerTitleCache::GetGroupTitleLayer(
    const tab_groups::TabGroupId& group_token,
    bool incognito) {
  auto it = group_layer_cache_.find(group_token);
  if (it != group_layer_cache_.end()) {
    return it->second.get();
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_LayerTitleCache_buildUpdatedGroupTitle(
      env, weak_java_title_cache_.get(env),
      base::android::TokenAndroid::Create(env, group_token.token()), incognito);

  // Retry the find.
  it = group_layer_cache_.find(group_token);
  return it == group_layer_cache_.end() ? nullptr : it->second.get();
}

LayerTitleCache::~LayerTitleCache() = default;

// ----------------------------------------------------------------------------
// Native JNI methods
// ----------------------------------------------------------------------------

jlong JNI_LayerTitleCache_Init(JNIEnv* env,
                               const JavaParamRef<jobject>& obj,
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
                               const JavaParamRef<jobject>& jresource_manager) {
  ui::ResourceManager* resource_manager =
      ui::ResourceManagerImpl::FromJavaObject(jresource_manager);
  LayerTitleCache* cache = new LayerTitleCache(
      env, obj, fade_width, icon_start_padding, icon_end_padding,
      spinner_resource_id, spinner_incognito_resource_id,
      bubble_inner_dimension, bubble_outer_dimension, bubble_offset,
      bubble_inner_tint, bubble_outer_tint, resource_manager);
  return reinterpret_cast<intptr_t>(cache);
}

}  // namespace android
