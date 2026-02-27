// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/widget/glic_view.h"

#include "base/command_line.h"
#include "base/metrics/user_metrics.h"
#include "chrome/browser/file_select_helper.h"
#include "chrome/browser/glic/fre/glic_fre_controller.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "components/input/native_web_keyboard_event.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/drop_data.h"
#include "third_party/blink/public/mojom/page/draggable_region.mojom.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_variant.h"
#include "ui/events/event_observer.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/views/background.h"
#include "ui/views/event_monitor.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace glic {

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(GlicView, kWebViewElementIdForTesting);

GlicView::GlicView(Profile* profile,
                   const gfx::Size& initial_size,
                   base::WeakPtr<ui::AcceleratorTarget> accelerator_delegate)
    : accelerator_delegate_(accelerator_delegate) {
  SetProperty(views::kElementIdentifierKey, kGlicViewElementId);
  SetLayoutManager(std::make_unique<views::FillLayout>());
  SetSize(initial_size);
  // As there is no WebContents yet, this will apply the default background.
  UpdateBackgroundColor();
}

GlicView::~GlicView() = default;

bool GlicView::HandleKeyboardEvent(content::WebContents* source,
                                   const input::NativeWebKeyboardEvent& event) {
  if (event.GetType() == input::NativeWebKeyboardEvent::Type::kRawKeyDown &&
      event.windows_key_code == ui::VKEY_ESCAPE) {
    if (auto* glic_service = GlicKeyedServiceFactory::GetGlicKeyedService(
            source->GetBrowserContext())) {
      if (glic_service->fre_controller().ShouldShowFreDialog() &&
          glic_service->IsWindowOrFreShowing()) {
        base::RecordAction(base::UserMetricsAction("Glic.Fre.CloseWithEsc"));
      }
    }
  }
  return GetWidget() && unhandled_keyboard_event_handler_.HandleKeyboardEvent(
                            event, GetWidget()->GetFocusManager());
}

void GlicView::RequestMediaAccessPermission(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback) {
  MediaCaptureDevicesDispatcher::GetInstance()->ProcessMediaAccessRequest(
      web_contents, request, std::move(callback), nullptr);
}

void GlicView::RunFileChooser(
    content::RenderFrameHost* render_frame_host,
    scoped_refptr<content::FileSelectListener> listener,
    const blink::mojom::FileChooserParams& params) {
  FileSelectHelper::RunFileChooser(render_frame_host, std::move(listener),
                                   params);
}

bool GlicView::CanDragEnter(content::WebContents* source,
                            const content::DropData& data,
                            blink::DragOperationsMask operations_allowed) {
  if (!base::FeatureList::IsEnabled(features::kGlicDragAndDropFileUpload)) {
    return false;
  }
  // Check for local files or URLs.
  bool has_files = !data.filenames.empty() || !data.file_system_files.empty();
  bool has_url = !data.url_infos.empty();

  return has_files || has_url;
}

void GlicView::SetWebContents(content::WebContents* web_contents) {
  views::WebView::SetWebContents(web_contents);
  if (web_contents) {
    web_contents->SetDelegate(this);
  }
}

void GlicView::DraggableRegionsChanged(
    const std::vector<blink::mojom::DraggableRegionPtr>& regions,
    content::WebContents* contents) {
  // `GlicView::DraggableRegionsChanged()` is called when draggable regions for
  // either the main-webcontents or guest-webcontents are changed.
  // guest-webcontents are the webcontents associated to `<webview>` hosting the
  // glic web app,
  SkRegion sk_region;
  for (const auto& region : regions) {
    sk_region.op(
        SkIRect::MakeLTRB(region->bounds.x(), region->bounds.y(),
                          region->bounds.right(), region->bounds.bottom()),
        region->draggable ? SkRegion::kUnion_Op : SkRegion::kDifference_Op);
  }

  SetDraggableRegion(sk_region);
}

bool GlicView::IsPointWithinDraggableRegion(const gfx::Point& point) {
  return draggable_region_.contains(point.x(), point.y());
}

void GlicView::UpdateBackgroundColor() {
  const bool explicit_background =
      base::FeatureList::IsEnabled(features::kGlicExplicitBackgroundColor);

  std::optional<ui::ColorVariant> background_color;
  if (!explicit_background) {
    background_color = GetClientBackgroundColor();
  }

  SetBackground(views::CreateLayerBasedRoundedBackground(
      background_color.value_or(kColorGlicBackground), background_radii_));
  background()->SetInternalName("GlicView/background");
  SetClipLayerToVisibleBounds(true);

  if (views::Widget* widget = GetWidget(); explicit_background && widget) {
    // Set the native widget background color if needed.
    widget->SetBackgroundColor(kColorGlicBackground);
  }
}

void GlicView::SetBackgroundRoundedCorners(const gfx::RoundedCornersF& radii) {
  if (radii == background_radii_) {
    return;
  }

  background_radii_ = radii;
  UpdateBackgroundColor();
}

bool GlicView::AcceleratorPressed(const ui::Accelerator& accelerator) {
  if (accelerator_delegate_) {
    return accelerator_delegate_->AcceleratorPressed(accelerator);
  }

  return false;
}

void GlicView::SetDraggableRegion(const SkRegion& region) {
  // Since <webview> covers the entire main web-contents, overriding the
  // draggable regions set by main web-contents (if any) is okay.
  draggable_region_ = region;
}

std::optional<SkColor> GlicView::GetClientBackgroundColor() {
  content::WebContents* host = GetWebContents();
  if (!host) {
    return std::nullopt;
  }

  std::vector<content::WebContents*> inner_contents =
      host->GetInnerWebContents();
  if (inner_contents.size() != 1ul) {
    return std::nullopt;
  }

  return inner_contents[0]->GetBackgroundColor();
}

BEGIN_METADATA(GlicView)
END_METADATA

}  // namespace glic
