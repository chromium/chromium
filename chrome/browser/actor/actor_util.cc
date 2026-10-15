// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_util.h"

#include <optional>
#include <vector>

#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_surface_handle.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/browser/glic/public/glic_instance.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/service/glic_instance_coordinator.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/actor/core/task_source_info.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "skia/ext/font_utils.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkFont.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/png_codec.h"

namespace actor {

glic::GlicInstance* GetGlicInstanceForTask(const ActorTask& task) {
  glic::GlicKeyedService* glic_service =
      glic::GlicKeyedService::Get(task.GetProfile());
  if (!glic_service) {
    return nullptr;
  }

  glic::GlicInstanceCoordinator& coordinator =
      glic_service->instance_coordinator();
  if (glic::GlicInstance* instance =
          coordinator.GetInstanceForTaskId(task.id().value())) {
    return instance;
  }

  const std::optional<std::string>& source_id = task.source_info().id;
  if (source_id.has_value() && !source_id->empty()) {
    if (glic::GlicInstance* instance =
            coordinator.GetInstanceForConversationId(*source_id)) {
      return instance;
    }
  }

  return nullptr;
}

bool HasGlicInstanceForTask(const ActorTask& task) {
  return GetGlicInstanceForTask(task) != nullptr;
}

bool HaveActiveTaskForContents(content::WebContents* source_contents) {
  if (!source_contents) {
    return false;
  }

  actor::ActorKeyedService* actor_service =
      actor::ActorKeyedService::Get(source_contents->GetBrowserContext());
  if (!actor_service) {
    return false;
  }

  return actor_service->GetActingActorTaskForWebContents(source_contents);
}

bool IsRunningBackgroundActorTask(content::WebContents& source_contents) {
  actor::ActorKeyedService* actor_service =
      actor::ActorKeyedService::Get(source_contents.GetBrowserContext());
  if (!actor_service) {
    return false;
  }

  const ActorTask* task =
      actor_service->GetActingActorTaskForWebContents(&source_contents);
  if (!task) {
    return false;
  }

  tabs::TabInterface* task_tab =
      tabs::TabInterface::GetFromContents(&source_contents);
  if (task_tab->IsActivated()) {
    // The tab in which the task is running is active, so the task isn't in the
    // background.
    return false;
  }

  // Determine whether the active tab is showing the conversation instance of
  // the actor task. If the conversation is showing, consider the task to be in
  // the foreground.

  glic::GlicKeyedService* glic_service =
      glic::GlicKeyedService::Get(source_contents.GetBrowserContext());
  if (!glic_service) {
    // Without Glic, there'd be no conversation to provide context to the user
    // while the tab isn't active, so consider the task backgrounded.
    return true;
  }

  const glic::GlicInstance* task_instance = GetGlicInstanceForTask(*task);
  if (!task_instance) {
    // There's no Glic instance for the task (e.g. something else is invoking
    // the actor framework). As above, there's no conversation, so consider the
    // task backgrounded.
    return true;
  }
  BrowserWindowInterface* task_window = task_tab->GetBrowserWindowInterface();
  const glic::GlicInstance* active_instance =
      glic_service->GetInstanceForActiveTab(task_window);
  if (task_instance != active_instance) {
    // The active tab has no Glic instance or an unrelated instance. The user
    // doesn't see the conversation related to the task, so consider the task
    // backgrounded.
    return true;
  }

  return !task_instance->IsShowing();
}

bool HasActorTaskPreventingNewWebContents(content::RenderFrameHost* rfh) {
  auto* wc = content::WebContents::FromRenderFrameHost(rfh);
  if (!wc) {
    return false;
  }

  auto* actor_service = ActorKeyedService::Get(wc->GetBrowserContext());
  if (!actor_service) {
    return false;
  }

  const ActorTask* task =
      actor_service->GetTaskFromActorSurface(ActorSurfaceHandle::From(wc));
  if (!task) {
    return false;
  }

  return !task->GetExecutionEngine().TabsCanOpenNewWebContents();
}

std::optional<std::vector<uint8_t>> GetScreenshotWithIframeBoundingBoxes(
    const std::vector<uint8_t>& screenshot_data,
    std::string_view mime_type,
    const optimization_guide::proto::ScreenshotInfo& screenshot_info) {
  if (screenshot_info.iframe_info_size() == 0) {
    return screenshot_data;
  }

  SkBitmap bitmap;
  if (mime_type == "image/png") {
    bitmap = gfx::PNGCodec::Decode(screenshot_data);
  } else {
    bitmap = gfx::JPEGCodec::Decode(screenshot_data);
  }

  if (bitmap.isNull()) {
    return std::nullopt;
  }

  SkCanvas canvas(bitmap);
  SkPaint paint;
  paint.setColor(SK_ColorRED);
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setStrokeWidth(3.0f);

  for (const auto& iframe : screenshot_info.iframe_info()) {
    if (iframe.has_bounding_box()) {
      const auto& rect = iframe.bounding_box();
      canvas.drawRect(
          SkRect::MakeXYWH(rect.x(), rect.y(), rect.width(), rect.height()),
          paint);

      if (iframe.has_security_origin() &&
          !iframe.security_origin().value().empty()) {
        SkFont font = skia::DefaultFont();
        font.setSize(12.0f);
        std::string origin_str = iframe.security_origin().value();
        SkRect text_bounds;
        font.measureText(origin_str.c_str(), origin_str.size(),
                         SkTextEncoding::kUTF8, &text_bounds);

        SkRect bg_rect = SkRect::MakeXYWH(
            rect.x() + 5.0f + text_bounds.left() - 2.0f,
            rect.y() + 15.0f + text_bounds.top() - 2.0f,
            text_bounds.width() + 4.0f, text_bounds.height() + 4.0f);

        SkPaint bg_paint;
        bg_paint.setColor(SK_ColorWHITE);
        bg_paint.setStyle(SkPaint::kFill_Style);
        canvas.drawRect(bg_rect, bg_paint);

        SkPaint text_paint;
        text_paint.setColor(SK_ColorRED);
        canvas.drawString(origin_str.c_str(), rect.x() + 5.0f, rect.y() + 15.0f,
                          font, text_paint);
      }
    }
  }

  std::optional<std::vector<uint8_t>> encoded;
  if (mime_type == "image/png") {
    encoded = gfx::PNGCodec::EncodeBGRASkBitmap(bitmap,
                                                /*discard_transparency=*/false);
  } else {
    encoded = gfx::JPEGCodec::Encode(bitmap, /*quality=*/100);
  }

  if (!encoded) {
    return std::nullopt;
  }
  return *encoded;
}

}  // namespace actor
