// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/scene_layer/tab_strip_scene_layer.h"

#include "base/android/jni_android.h"
#include "cc/resources/scoped_ui_resource.h"
#include "chrome/android/chrome_jni_headers/TabStripSceneLayer_jni.h"
#include "chrome/browser/android/compositor/layer/tab_handle_layer.h"
#include "chrome/browser/android/compositor/layer_title_cache.h"
#include "ui/android/resources/nine_patch_resource.h"
#include "ui/android/resources/resource_manager_impl.h"
#include "ui/gfx/transform.h"

using base::android::JavaParamRef;
using base::android::JavaRef;

namespace android {

TabStripSceneLayer::TabStripSceneLayer(JNIEnv* env,
                                       const JavaRef<jobject>& jobj)
    : SceneLayer(env, jobj),
      tab_strip_layer_(cc::SolidColorLayer::Create()),
      scrollable_strip_layer_(cc::Layer::Create()),
      new_tab_button_(cc::UIResourceLayer::Create()),
      left_fade_(cc::UIResourceLayer::Create()),
      right_fade_(cc::UIResourceLayer::Create()),
      model_selector_button_(cc::UIResourceLayer::Create()),
      background_tab_brightness_(1.f),
      brightness_(1.f),
      write_index_(0),
      content_tree_(nullptr) {
  new_tab_button_->SetIsDrawable(true);
  model_selector_button_->SetIsDrawable(true);
  left_fade_->SetIsDrawable(true);
  right_fade_->SetIsDrawable(true);

  // When the ScrollingStripStacker is used, the new tab button and tabs scroll,
  // while the incognito button and left/ride fade stay fixed. Put the new tab
  // button and tabs in a separate layer placed visually below the others.
  scrollable_strip_layer_->SetIsDrawable(true);
  scrollable_strip_layer_->AddChild(new_tab_button_);

  tab_strip_layer_->SetBackgroundColor(SK_ColorBLACK);
  tab_strip_layer_->SetIsDrawable(true);
  tab_strip_layer_->AddChild(scrollable_strip_layer_);
  tab_strip_layer_->AddChild(left_fade_);
  tab_strip_layer_->AddChild(right_fade_);
  tab_strip_layer_->AddChild(model_selector_button_);
  layer()->AddChild(tab_strip_layer_);
}

TabStripSceneLayer::~TabStripSceneLayer() {
}

void TabStripSceneLayer::SetContentTree(
    JNIEnv* env,
    const JavaParamRef<jobject>& jobj,
    const JavaParamRef<jobject>& jcontent_tree) {
  SceneLayer* content_tree = FromJavaObject(env, jcontent_tree);
  if (content_tree_ &&
      (!content_tree_->layer()->parent() ||
       content_tree_->layer()->parent()->id() != layer()->id()))
    content_tree_ = nullptr;

  if (content_tree != content_tree_) {
    if (content_tree_)
      content_tree_->layer()->RemoveFromParent();
    content_tree_ = content_tree;
    if (content_tree) {
      layer()->InsertChild(content_tree->layer(), 0);
      content_tree->layer()->SetPosition(
          gfx::PointF(0, -layer()->position().y()));
    }
  }
}

void TabStripSceneLayer::BeginBuildingFrame(JNIEnv* env,
                                            const JavaParamRef<jobject>& jobj,
                                            jboolean visible) {
  write_index_ = 0;
  tab_strip_layer_->SetHideLayerAndSubtree(!visible);
}

void TabStripSceneLayer::FinishBuildingFrame(
    JNIEnv* env,
    const JavaParamRef<jobject>& jobj) {
  if (tab_strip_layer_->hide_layer_and_subtree())
    return;

  for (unsigned i = write_index_; i < tab_handle_layers_.size(); ++i)
    tab_handle_layers_[i]->layer()->RemoveFromParent();

  tab_handle_layers_.erase(tab_handle_layers_.begin() + write_index_,
                           tab_handle_layers_.end());
}

void TabStripSceneLayer::UpdateTabStripLayer(JNIEnv* env,
                                             const JavaParamRef<jobject>& jobj,
                                             jfloat width,
                                             jfloat height,
                                             jfloat y_offset,
                                             jfloat background_tab_brightness,
                                             jfloat brightness,
                                             jboolean should_readd_background) {
  background_tab_brightness_ = background_tab_brightness;
  gfx::RectF content(0, y_offset, width, height);
  layer()->SetPosition(gfx::PointF(0, y_offset));
  tab_strip_layer_->SetBounds(gfx::Size(width, height));
  scrollable_strip_layer_->SetBounds(gfx::Size(width, height));

  if (brightness != brightness_) {
    brightness_ = brightness;
    cc::FilterOperations filters;
    if (brightness_ < 1.f)
      filters.Append(cc::FilterOperation::CreateBrightnessFilter(brightness_));
    tab_strip_layer_->SetFilters(filters);
  }

  // Content tree should not be affected by tab strip scene layer visibility.
  if (content_tree_)
    content_tree_->layer()->SetPosition(gfx::PointF(0, -y_offset));

  // Make sure tab strip changes are committed after rotating the device.
  // See https://crbug.com/503930 for more details.
  // InsertChild() forces the tree sync, which seems to fix the problem.
  // Note that this is a workaround.
  // TODO(changwan): find out why the update is not committed after rotation.
  if (should_readd_background) {
    int background_index = 0;
    if (content_tree_ && content_tree_->layer()) {
      background_index = 1;
    }
    DCHECK(layer()->children()[background_index] == tab_strip_layer_);
    layer()->InsertChild(tab_strip_layer_, background_index);
  }
}

void TabStripSceneLayer::UpdateNewTabButton(
    JNIEnv* env,
    const JavaParamRef<jobject>& jobj,
    jint resource_id,
    jfloat x,
    jfloat y,
    jfloat width,
    jfloat height,
    jboolean visible,
    const JavaParamRef<jobject>& jresource_manager) {
  ui::ResourceManager* resource_manager =
      ui::ResourceManagerImpl::FromJavaObject(jresource_manager);
  ui::Resource* button_resource = resource_manager->GetResource(
      ui::ANDROID_RESOURCE_TYPE_STATIC, resource_id);

  new_tab_button_->SetUIResourceId(button_resource->ui_resource()->id());
  float left_offset = (width - button_resource->size().width()) / 2;
  float top_offset = (height - button_resource->size().height()) / 2;
  new_tab_button_->SetPosition(gfx::PointF(x + left_offset, y + top_offset));
  new_tab_button_->SetBounds(button_resource->size());
  new_tab_button_->SetHideLayerAndSubtree(!visible);
}

void TabStripSceneLayer::UpdateModelSelectorButton(
    JNIEnv* env,
    const JavaParamRef<jobject>& jobj,
    jint resource_id,
    jfloat x,
    jfloat y,
    jfloat width,
    jfloat height,
    jboolean incognito,
    jboolean visible,
    const JavaParamRef<jobject>& jresource_manager) {
  ui::ResourceManager* resource_manager =
      ui::ResourceManagerImpl::FromJavaObject(jresource_manager);
  ui::Resource* button_resource = resource_manager->GetResource(
      ui::ANDROID_RESOURCE_TYPE_STATIC, resource_id);

  model_selector_button_->SetUIResourceId(button_resource->ui_resource()->id());
  float left_offset = (width - button_resource->size().width()) / 2;
  float top_offset = (height - button_resource->size().height()) / 2;
  model_selector_button_->SetPosition(
      gfx::PointF(x + left_offset, y + top_offset));
  model_selector_button_->SetBounds(button_resource->size());
  model_selector_button_->SetHideLayerAndSubtree(!visible);
}

void TabStripSceneLayer::UpdateTabStripLeftFade(
    JNIEnv* env,
    const JavaParamRef<jobject>& jobj,
    jint resource_id,
    jfloat opacity,
    const JavaParamRef<jobject>& jresource_manager) {

  // Hide layer if it's not visible.
  if (opacity == 0.f) {
    left_fade_->SetHideLayerAndSubtree(true);
    return;
  }

  // Set UI resource.
  ui::ResourceManager* resource_manager =
      ui::ResourceManagerImpl::FromJavaObject(jresource_manager);
  ui::Resource* fade_resource = resource_manager->GetResource(
      ui::ANDROID_RESOURCE_TYPE_STATIC, resource_id);
  left_fade_->SetUIResourceId(fade_resource->ui_resource()->id());

  // The same resource is used for both left and right fade, so the
  // resource must be rotated for the left fade.
  gfx::Transform fade_transform;
  fade_transform.RotateAboutYAxis(180.0);
  left_fade_->SetTransform(fade_transform);

  // Set opacity.
  left_fade_->SetOpacity(opacity);

  // Set bounds. Use the parent layer height so the 1px fade resource is
  // stretched vertically.
  left_fade_->SetBounds(gfx::Size(fade_resource->size().width(),
                                  scrollable_strip_layer_->bounds().height()));

  // Set position. The rotation set above requires the layer to be offset
  // by its width in order to display on the left edge.
  left_fade_->SetPosition(gfx::PointF(fade_resource->size().width(), 0));

  // Ensure layer is visible.
  left_fade_->SetHideLayerAndSubtree(false);
}

void TabStripSceneLayer::UpdateTabStripRightFade(
    JNIEnv* env,
    const JavaParamRef<jobject>& jobj,
    jint resource_id,
    jfloat opacity,
    const JavaParamRef<jobject>& jresource_manager) {

  // Hide layer if it's not visible.
  if (opacity == 0.f) {
    right_fade_->SetHideLayerAndSubtree(true);
    return;
  }

  // Set UI resource.
  ui::ResourceManager* resource_manager =
      ui::ResourceManagerImpl::FromJavaObject(jresource_manager);
  ui::Resource* fade_resource = resource_manager->GetResource(
      ui::ANDROID_RESOURCE_TYPE_STATIC, resource_id);
  right_fade_->SetUIResourceId(fade_resource->ui_resource()->id());

  // Set opacity.
  right_fade_->SetOpacity(opacity);

  // Set bounds. Use the parent layer height so the 1px fade resource is
  // stretched vertically.
  right_fade_->SetBounds(gfx::Size(fade_resource->size().width(),
                                   scrollable_strip_layer_->bounds().height()));

  // Set position. The right fade is positioned at the end of the tab strip.
  float x =
      scrollable_strip_layer_->bounds().width() - fade_resource->size().width();
  right_fade_->SetPosition(gfx::PointF(x, 0));

  // Ensure layer is visible.
  right_fade_->SetHideLayerAndSubtree(false);
}

void TabStripSceneLayer::PutStripTabLayer(
    JNIEnv* env,
    const JavaParamRef<jobject>& jobj,
    jint id,
    jint close_resource_id,
    jint handle_resource_id,
    jint handle_outline_resource_id,
    jint close_tint,
    jint handle_tint,
    jint handle_outline_tint,
    jboolean foreground,
    jboolean close_pressed,
    jfloat toolbar_width,
    jfloat x,
    jfloat y,
    jfloat width,
    jfloat height,
    jfloat content_offset_x,
    jfloat close_button_alpha,
    jboolean is_loading,
    jfloat spinner_rotation,
    const JavaParamRef<jobject>& jlayer_title_cache,
    const JavaParamRef<jobject>& jresource_manager) {
  LayerTitleCache* layer_title_cache =
      LayerTitleCache::FromJavaObject(jlayer_title_cache);
  ui::ResourceManager* resource_manager =
      ui::ResourceManagerImpl::FromJavaObject(jresource_manager);
  scoped_refptr<TabHandleLayer> layer = GetNextLayer(layer_title_cache);
  ui::NinePatchResource* tab_handle_resource =
      ui::NinePatchResource::From(resource_manager->GetStaticResourceWithTint(
          handle_resource_id, handle_tint));
  ui::NinePatchResource* tab_handle_outline_resource =
      ui::NinePatchResource::From(resource_manager->GetStaticResourceWithTint(
          handle_outline_resource_id, handle_outline_tint));
  ui::Resource* close_button_resource =
      resource_manager->GetStaticResourceWithTint(close_resource_id,
                                                  close_tint);
  layer->SetProperties(id, close_button_resource, tab_handle_resource,
                       tab_handle_outline_resource, foreground, close_pressed,
                       toolbar_width, x, y, width, height, content_offset_x,
                       close_button_alpha, is_loading, spinner_rotation,
                       background_tab_brightness_);
}

scoped_refptr<TabHandleLayer> TabStripSceneLayer::GetNextLayer(
    LayerTitleCache* layer_title_cache) {
  if (write_index_ < tab_handle_layers_.size())
    return tab_handle_layers_[write_index_++];

  scoped_refptr<TabHandleLayer> layer_tree =
      TabHandleLayer::Create(layer_title_cache);
  tab_handle_layers_.push_back(layer_tree);
  scrollable_strip_layer_->AddChild(layer_tree->layer());
  write_index_++;
  return layer_tree;
}

bool TabStripSceneLayer::ShouldShowBackground() {
  if (content_tree_)
    return content_tree_->ShouldShowBackground();
  return SceneLayer::ShouldShowBackground();
}

SkColor TabStripSceneLayer::GetBackgroundColor() {
  if (content_tree_)
    return content_tree_->GetBackgroundColor();
  return SceneLayer::GetBackgroundColor();
}

static jlong JNI_TabStripSceneLayer_Init(JNIEnv* env,
                                         const JavaParamRef<jobject>& jobj) {
  // This will automatically bind to the Java object and pass ownership there.
  TabStripSceneLayer* scene_layer = new TabStripSceneLayer(env, jobj);
  return reinterpret_cast<intptr_t>(scene_layer);
}

}  // namespace android
