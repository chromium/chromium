// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geic/geic_view.h"

#include <cmath>
#include <optional>

#include "base/containers/adapters.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/pwc/privileged_web_contents.h"
#include "components/input/native_web_keyboard_event.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_zoom.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/focus/focus_manager.h"

namespace geic {

namespace {

std::optional<double> FindNextZoomInFactor(double current_factor) {
  int current_percent = static_cast<int>(std::round(current_factor * 100.0));
  for (double factor : GeicView::kZoomFactors) {
    int factor_percent = static_cast<int>(std::round(factor * 100.0));
    if (factor_percent > current_percent) {
      return factor;
    }
  }
  return std::nullopt;
}

std::optional<double> FindNextZoomOutFactor(double current_factor) {
  int current_percent = static_cast<int>(std::round(current_factor * 100.0));
  for (double factor : base::Reversed(GeicView::kZoomFactors)) {
    int factor_percent = static_cast<int>(std::round(factor * 100.0));
    if (factor_percent < current_percent) {
      return factor;
    }
  }
  return std::nullopt;
}

bool IsZoomAccelerator(const ui::Accelerator& accelerator) {
  if ((accelerator.modifiers() & ui::EF_PLATFORM_ACCELERATOR) == 0) {
    return false;
  }
  switch (accelerator.key_code()) {
    case ui::VKEY_OEM_PLUS:
    case ui::VKEY_ADD:
    case ui::VKEY_OEM_MINUS:
    case ui::VKEY_SUBTRACT:
    case ui::VKEY_0:
    case ui::VKEY_NUMPAD0:
      return true;
    default:
      return false;
  }
}

}  // namespace

GeicView::GeicView(Profile* profile) : views::WebView(profile) {
  SetID(kGeicWebViewId);

  // Zoom In
  AddAccelerator(
      ui::Accelerator(ui::VKEY_OEM_PLUS, ui::EF_PLATFORM_ACCELERATOR));
  AddAccelerator(ui::Accelerator(
      ui::VKEY_OEM_PLUS, ui::EF_PLATFORM_ACCELERATOR | ui::EF_SHIFT_DOWN));
  AddAccelerator(ui::Accelerator(ui::VKEY_ADD, ui::EF_PLATFORM_ACCELERATOR));
  AddAccelerator(ui::Accelerator(
      ui::VKEY_ADD, ui::EF_PLATFORM_ACCELERATOR | ui::EF_SHIFT_DOWN));

  // Zoom Out
  AddAccelerator(
      ui::Accelerator(ui::VKEY_OEM_MINUS, ui::EF_PLATFORM_ACCELERATOR));
  AddAccelerator(ui::Accelerator(
      ui::VKEY_OEM_MINUS, ui::EF_PLATFORM_ACCELERATOR | ui::EF_SHIFT_DOWN));
  AddAccelerator(
      ui::Accelerator(ui::VKEY_SUBTRACT, ui::EF_PLATFORM_ACCELERATOR));
  AddAccelerator(ui::Accelerator(
      ui::VKEY_SUBTRACT, ui::EF_PLATFORM_ACCELERATOR | ui::EF_SHIFT_DOWN));

  // Zoom Reset
  AddAccelerator(ui::Accelerator(ui::VKEY_0, ui::EF_PLATFORM_ACCELERATOR));
  AddAccelerator(ui::Accelerator(
      ui::VKEY_0, ui::EF_PLATFORM_ACCELERATOR | ui::EF_SHIFT_DOWN));
  AddAccelerator(
      ui::Accelerator(ui::VKEY_NUMPAD0, ui::EF_PLATFORM_ACCELERATOR));
  AddAccelerator(ui::Accelerator(
      ui::VKEY_NUMPAD0, ui::EF_PLATFORM_ACCELERATOR | ui::EF_SHIFT_DOWN));
}

GeicView::~GeicView() {
  if (web_contents()) {
    if (auto* pwc =
            pwc::PrivilegedWebContents::FromWebContents(web_contents())) {
      if (pwc->embedder_delegate() == this) {
        pwc->SetEmbedderDelegate(nullptr);
      }
    }
  }
}

void GeicView::SetWebContents(content::WebContents* new_web_contents) {
  if (web_contents()) {
    if (auto* old_pwc =
            pwc::PrivilegedWebContents::FromWebContents(web_contents())) {
      if (old_pwc->embedder_delegate() == this) {
        old_pwc->SetEmbedderDelegate(nullptr);
      }
    }
  }

  views::WebView::SetWebContents(new_web_contents);

  if (new_web_contents) {
    if (!zoom::ZoomController::FromWebContents(new_web_contents)) {
      zoom::ZoomController::CreateForWebContents(new_web_contents);
    }
    auto* zoom_controller =
        zoom::ZoomController::FromWebContents(new_web_contents);
    zoom_controller->SetZoomMode(zoom::ZoomController::ZOOM_MODE_ISOLATED);
    zoom_controller->SetShowsNotificationBubble(false);

    if (auto* new_pwc =
            pwc::PrivilegedWebContents::FromWebContents(new_web_contents)) {
      new_pwc->SetEmbedderDelegate(this);
    }
  }
}

void GeicView::Zoom(content::PageZoom zoom) {
  if (!web_contents()) {
    return;
  }
  auto* zoom_controller = zoom::ZoomController::FromWebContents(web_contents());
  if (!zoom_controller) {
    return;
  }

  double current_factor = GetZoomFactor();
  int current_percent = static_cast<int>(std::round(current_factor * 100.0));

  std::optional<double> target_factor;
  switch (zoom) {
    case content::PAGE_ZOOM_IN:
      if (current_percent < 200) {
        target_factor = FindNextZoomInFactor(current_factor);
      }
      break;
    case content::PAGE_ZOOM_OUT:
      if (current_percent > 100) {
        target_factor = FindNextZoomOutFactor(current_factor);
      }
      break;
    case content::PAGE_ZOOM_RESET:
      target_factor = 1.0;
      break;
  }

  if (target_factor) {
    double target_level = blink::ZoomFactorToZoomLevel(*target_factor);
    zoom_controller->SetZoomLevel(target_level);
    if (zoom == content::PAGE_ZOOM_RESET) {
      web_contents()->SetPageScale(1.f);
    }
  }
}

double GeicView::GetZoomFactor() const {
  if (!web_contents()) {
    return 1.0;
  }
  auto* zoom_controller = zoom::ZoomController::FromWebContents(web_contents());
  if (!zoom_controller) {
    return 1.0;
  }
  return blink::ZoomLevelToZoomFactor(zoom_controller->GetZoomLevel());
}

bool GeicView::AcceleratorPressed(const ui::Accelerator& accelerator) {
  if (!web_contents() || !IsZoomAccelerator(accelerator)) {
    return false;
  }
  switch (accelerator.key_code()) {
    case ui::VKEY_OEM_PLUS:
    case ui::VKEY_ADD:
      Zoom(content::PAGE_ZOOM_IN);
      return true;
    case ui::VKEY_OEM_MINUS:
    case ui::VKEY_SUBTRACT:
      Zoom(content::PAGE_ZOOM_OUT);
      return true;
    case ui::VKEY_0:
    case ui::VKEY_NUMPAD0:
      Zoom(content::PAGE_ZOOM_RESET);
      return true;
    default:
      return false;
  }
}

bool GeicView::CanHandleAccelerators() const {
  if (!views::WebView::CanHandleAccelerators()) {
    return false;
  }
  return ContainsOrHasFocus();
}

bool GeicView::HandleKeyboardEvent(content::WebContents* source,
                                   const input::NativeWebKeyboardEvent& event) {
  views::FocusManager* focus_manager = GetFocusManager();
  return focus_manager && unhandled_keyboard_event_handler_.HandleKeyboardEvent(
                              event, focus_manager);
}

void GeicView::ContentsZoomChange(bool zoom_in) {
  Zoom(zoom_in ? content::PAGE_ZOOM_IN : content::PAGE_ZOOM_OUT);
}

bool GeicView::ContainsOrHasFocus() const {
  const views::FocusManager* focus_manager = GetFocusManager();
  return focus_manager && Contains(focus_manager->GetFocusedView());
}

BEGIN_METADATA(GeicView)
END_METADATA

}  // namespace geic
