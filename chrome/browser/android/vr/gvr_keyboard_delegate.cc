// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/gvr_keyboard_delegate.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/vr/gvr_util.h"
#include "chrome/browser/vr/model/camera_model.h"
#include "chrome/browser/vr/model/text_input_info.h"
#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr.h"
#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr_types.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/vector3d_f.h"

// This method is supplied by the VR keyboard shim, but is not part of the
// GVR interface.
bool gvr_keyboard_supports_selection();
int64_t gvr_keyboard_version();

namespace vr {

// The minimum keyboard version required for the needed features to work.
constexpr int64_t kMinRequiredApiVersion = 2;

namespace {

void OnKeyboardEvent(void* closure, GvrKeyboardDelegate::EventType event) {
  auto* callback =
      reinterpret_cast<GvrKeyboardDelegate::OnEventCallback*>(closure);
  if (callback)
    callback->Run(event);
}

}  // namespace

std::unique_ptr<GvrKeyboardDelegate> GvrKeyboardDelegate::Create() {
  auto delegate = base::WrapUnique(new GvrKeyboardDelegate());
  void* callback = reinterpret_cast<void*>(&delegate->keyboard_event_callback_);
  auto* gvr_keyboard = gvr_keyboard_create(callback, OnKeyboardEvent);
  if (!gvr_keyboard)
    return nullptr;

  if (gvr_keyboard_version() < kMinRequiredApiVersion) {
    gvr_keyboard_destroy(&gvr_keyboard);
    return nullptr;
  }

  delegate->Init(gvr_keyboard);
  return delegate;
}

GvrKeyboardDelegate::GvrKeyboardDelegate() {
  keyboard_event_callback_ = base::BindRepeating(
      &GvrKeyboardDelegate::OnGvrKeyboardEvent, base::Unretained(this));
}

void GvrKeyboardDelegate::Init(gvr_keyboard_context* keyboard_context) {
  DCHECK(keyboard_context);
  gvr_keyboard_ = keyboard_context;

  gvr_mat4f matrix;
  gvr_keyboard_get_recommended_world_from_keyboard_matrix(2.0f, &matrix);
  gvr_keyboard_set_world_from_keyboard_matrix(gvr_keyboard_, &matrix);
}

GvrKeyboardDelegate::~GvrKeyboardDelegate() {
  if (gvr_keyboard_)
    gvr_keyboard_destroy(&gvr_keyboard_);
}

void GvrKeyboardDelegate::SetUiInterface(KeyboardUiInterface* ui) {
  ui_ = ui;
}

void GvrKeyboardDelegate::OnBeginFrame() {
  // Pause keyboard updates until previous updates from the keyboard are acked.
  if (pause_keyboard_update_)
    return;

  gvr::ClockTimePoint target_time = gvr::GvrApi::GetTimePointNow();
  gvr_keyboard_set_frame_time(gvr_keyboard_, &target_time);
  gvr_keyboard_advance_frame(gvr_keyboard_);
}

void GvrKeyboardDelegate::ShowKeyboard() {
  gvr_keyboard_show(gvr_keyboard_);
}

void GvrKeyboardDelegate::HideKeyboard() {
  gvr_keyboard_hide(gvr_keyboard_);
}

void GvrKeyboardDelegate::SetTransform(const gfx::Transform& transform) {
  gvr_mat4f matrix;
  TransformToGvrMat(transform, &matrix);
  gvr_keyboard_set_world_from_keyboard_matrix(gvr_keyboard_, &matrix);
}

bool GvrKeyboardDelegate::HitTest(const gfx::Point3F& ray_origin,
                                  const gfx::Point3F& ray_target,
                                  gfx::Point3F* hit_position) {
  gvr_vec3f start;
  start.x = ray_origin.x();
  start.y = ray_origin.y();
  start.z = ray_origin.z();
  gvr_vec3f end;
  end.x = ray_target.x();
  end.y = ray_target.y();
  end.z = ray_target.z();
  gvr_vec3f hit_point;
  bool hits = gvr_keyboard_update_controller_ray(gvr_keyboard_, &start, &end,
                                                 &hit_point);
  if (hits)
    hit_position->SetPoint(hit_point.x, hit_point.y, hit_point.z);
  return hits;
}

void GvrKeyboardDelegate::Draw(const CameraModel& model) {
  int eye = model.eye_type;
  gvr::Mat4f view_matrix;
  TransformToGvrMat(model.view_matrix, &view_matrix);
  gvr_keyboard_set_eye_from_world_matrix(gvr_keyboard_, eye, &view_matrix);

  gvr::Mat4f proj_matrix;
  TransformToGvrMat(model.proj_matrix, &proj_matrix);
  gvr_keyboard_set_projection_matrix(gvr_keyboard_, eye, &proj_matrix);

  gfx::Rect viewport_rect = model.viewport;
  const gvr::Recti viewport = {viewport_rect.x(), viewport_rect.right(),
                               viewport_rect.y(), viewport_rect.bottom()};
  gvr_keyboard_set_viewport(gvr_keyboard_, eye, &viewport);
  gvr_keyboard_render(gvr_keyboard_, eye);
}

bool GvrKeyboardDelegate::SupportsSelection() {
  return gvr_keyboard_supports_selection();
}

void GvrKeyboardDelegate::OnTouchStateUpdated(
    bool is_touching,
    const gfx::PointF& touch_position) {
  gvr::Vec2f position = {touch_position.x(), touch_position.y()};
  gvr_keyboard_update_controller_touch(gvr_keyboard_, is_touching, &position);
}

void GvrKeyboardDelegate::OnButtonDown(const gfx::PointF& position) {
  gvr_keyboard_update_button_state(
      gvr_keyboard_, gvr::ControllerButton::GVR_CONTROLLER_BUTTON_CLICK, true);
}

void GvrKeyboardDelegate::OnButtonUp(const gfx::PointF& position) {
  gvr_keyboard_update_button_state(
      gvr_keyboard_, gvr::ControllerButton::GVR_CONTROLLER_BUTTON_CLICK, false);
}

void GvrKeyboardDelegate::UpdateInput(const TextInputInfo& info) {
  cached_text_input_info_ = info;

  // Gvr doesn't like inverted selections, so un-invert them for Gvr.
  if (cached_text_input_info_.selection_start >
      cached_text_input_info_.selection_end) {
    std::swap(cached_text_input_info_.selection_start,
              cached_text_input_info_.selection_end);
  }

  gvr_keyboard_set_text(
      gvr_keyboard_, base::UTF16ToUTF8(cached_text_input_info_.text).c_str());
  gvr_keyboard_set_selection_indices(gvr_keyboard_,
                                     cached_text_input_info_.selection_start,
                                     cached_text_input_info_.selection_end);
  gvr_keyboard_set_composing_indices(gvr_keyboard_,
                                     cached_text_input_info_.composition_start,
                                     cached_text_input_info_.composition_end);
  pause_keyboard_update_ = false;
}

void GvrKeyboardDelegate::OnGvrKeyboardEvent(EventType event) {
  DCHECK(ui_ != nullptr);
  switch (event) {
    case GVR_KEYBOARD_ERROR_UNKNOWN:
      LOG(ERROR) << "Unknown GVR keyboard error.";
      break;
    case GVR_KEYBOARD_ERROR_SERVICE_NOT_CONNECTED:
      LOG(ERROR) << "GVR keyboard service not connected.";
      break;
    case GVR_KEYBOARD_ERROR_NO_LOCALES_FOUND:
      LOG(ERROR) << "No GVR keyboard locales found.";
      break;
    case GVR_KEYBOARD_ERROR_SDK_LOAD_FAILED:
      LOG(ERROR) << "GVR keyboard sdk load failed.";
      break;
    case GVR_KEYBOARD_SHOWN:
      break;
    case GVR_KEYBOARD_HIDDEN:
      ui_->OnKeyboardHidden();
      break;
    case GVR_KEYBOARD_TEXT_UPDATED: {
      auto info = GetTextInfo();
      DCHECK(!pause_keyboard_update_);
      if (info != cached_text_input_info_) {
        ui_->OnInputEdited(EditedText(info, cached_text_input_info_));
        pause_keyboard_update_ = true;
      }
      break;
    }
    case GVR_KEYBOARD_TEXT_COMMITTED:
      ui_->OnInputCommitted(EditedText(GetTextInfo(), cached_text_input_info_));
      break;
  }
}

TextInputInfo GvrKeyboardDelegate::GetTextInfo() {
  TextInputInfo info;
  // Get text. Note that we wrap the text in a unique ptr since we're
  // responsible for freeing the memory allocated by gvr_keyboard_get_text.
  std::unique_ptr<char, decltype(std::free)*> scoped_text{
      gvr_keyboard_get_text(gvr_keyboard_), std::free};
  std::string text(scoped_text.get());
  info.text = base::UTF8ToUTF16(text);
  // Get selection indices.
  size_t start, end;
  gvr_keyboard_get_selection_indices(gvr_keyboard_, &start, &end);
  info.selection_start = start;
  info.selection_end = end;
  gvr_keyboard_get_composing_indices(gvr_keyboard_, &start, &end);
  info.composition_start = start;
  info.composition_end = end;
  if (info.composition_start == info.composition_end) {
    info.composition_start = TextInputInfo::kDefaultCompositionIndex;
    info.composition_end = TextInputInfo::kDefaultCompositionIndex;
  }
  return info;
}

}  // namespace vr
