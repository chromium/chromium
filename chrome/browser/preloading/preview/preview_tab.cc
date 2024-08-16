// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/preview/preview_tab.h"

#include "base/features.h"
#include "build/buildflag.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_initialize.h"
#include "chrome/browser/preloading/preview/preview_manager.h"
#include "chrome/browser/preloading/preview/preview_zoom_controller.h"
#include "chrome/browser/ssl/chrome_security_state_tab_helper.h"
#include "chrome/browser/ui/tab_helpers.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/preview_cancel_reason.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/mojom/window_features/window_features.mojom.h"
#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif  // defined(USE_AURA)
#include "ui/base/page_transition_types.h"
#include "ui/base/ui_base_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

namespace {

content::WebContents::CreateParams CreateWebContentsCreateParams(
    content::BrowserContext* context) {
  CHECK(context);
  content::WebContents::CreateParams params(context);
  params.preview_mode = true;
  return params;
}

std::unique_ptr<views::Widget> CreateWidget(content::WebContents& parent,
                                            views::View* view,
                                            views::WidgetObserver* observer) {
  // TODO(b:292184832): Create with own buttons

  views::Widget::InitParams params(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW);
  params.shadow_type = views::Widget::InitParams::ShadowType::kDrop;
  const gfx::Rect& rect = parent.GetViewBounds();
  params.bounds =
      gfx::Rect(rect.x() + rect.width() / 2, rect.y() + rect.height() / 2,
                rect.width() / 2, rect.height() / 2);
#if BUILDFLAG(IS_OZONE)
  params.use_accelerated_widget_override = true;
#endif

  std::unique_ptr<views::Widget> widget =
      std::make_unique<views::Widget>(std::move(params));
  widget->non_client_view()->frame_view()->SetLayoutManager(
      std::make_unique<views::FillLayout>());
  widget->non_client_view()->frame_view()->InsertClientView(
      new views::ClientView(widget.get(), view));
  widget->AddObserver(observer);
  widget->Show();
  return widget;
}

}  // namespace

PreviewTab::PreviewTab(PreviewManager* preview_manager,
                       content::WebContents& initiator_web_contents,
                       const GURL& url)
    : preview_manager_(preview_manager),
      web_contents_(content::WebContents::Create(CreateWebContentsCreateParams(
          initiator_web_contents.GetBrowserContext()))),
      view_(std::make_unique<views::WebView>(nullptr)),
      url_(url) {
  CHECK(base::FeatureList::IsEnabled(blink::features::kLinkPreview));
  web_contents_->SetDelegate(this);
  scoped_ignore_web_inputs_ =
      web_contents_->IgnoreInputEvents(base::BindRepeating(
          &PreviewTab::AuditWebInputEvent, base::Unretained(this)));

  // WebView setup.
  view_->SetWebContents(web_contents_.get());

  AttachTabHelpersForInit();
  // See the comment of PreviewZoomController for creation order.
  preview_zoom_controller_ =
      std::make_unique<PreviewZoomController>(web_contents_.get());

  // TODO(b:292184832): Ensure if we provide enough information to perform an
  // equivalent navigation with a link navigation.
  view_->LoadInitialURL(url_);

  widget_ =
      CreateWidget(initiator_web_contents, view_.get(), /*observer=*/this);

  // Register keyboard accelerators. Should be called after the window is
  // prepared above.
  RegisterKeyboardAccelerators();
}

PreviewTab::~PreviewTab() = default;

base::WeakPtr<content::WebContents> PreviewTab::GetWebContents() {
  if (!web_contents_) {
    return nullptr;
  }

  return web_contents_->GetWeakPtr();
}

void PreviewTab::AttachTabHelpersForInit() {
  content::WebContents* web_contents = web_contents_.get();

  // TODO(b:291867757): Audit TabHelpers and determine when
  // (initiation/promotion) we should attach each of them.
  zoom::ZoomController::CreateForWebContents(web_contents);
  ChromeSecurityStateTabHelper::CreateForWebContents(web_contents);
  chrome::InitializePageLoadMetricsForWebContents(web_contents);
}

bool PreviewTab::AuditWebInputEvent(const blink::WebInputEvent& event) {
  // Permit only page scroll related events.
  // TODO(b:329147054): Revisit to support touch devices, and care for web
  // exposed behaviors' compatibility.
  const blink::WebInputEvent::Type type = event.GetType();
  if (type == blink::WebInputEvent::Type::kMouseWheel ||
      type == blink::WebInputEvent::Type::kGestureScrollBegin ||
      type == blink::WebInputEvent::Type::kGestureScrollEnd ||
      type == blink::WebInputEvent::Type::kGestureScrollUpdate) {
    return true;
  }
  // Activate by any mouse down as window focus also changes by mouse down.
  if (type == blink::WebInputEvent::Type::kMouseDown) {
    preview_manager_->PromoteToNewTab();
  }
  return false;
}

content::PreloadingEligibility PreviewTab::IsPrerender2Supported(
    content::WebContents& web_contents) {
  return content::PreloadingEligibility::kPreloadingDisabled;
}

void PreviewTab::CancelPreview(content::PreviewCancelReason reason) {
  // TODO(b:299240273): Show an error page when final status is
  // kBlockedByMojoBinderPolicy.
  cancel_reason_ = std::move(reason);
}

void PreviewTab::PromoteToNewTab(content::WebContents& initiator_web_contents) {
  // If preview failed, prevent activation and just close the preview window.
  //
  // Currently, PreviewFinalStatus::kBlockedByMojoBinderPolicy contains just
  // deferred cases and we don't reject activation here.
  //
  // TODO(b:316226787): Consider to split the final status into
  // cancelled/deferred.
  if (cancel_reason_.has_value() &&
      cancel_reason_->GetFinalStatus() !=
          content::PreviewFinalStatus::kBlockedByMojoBinderPolicy) {
    return;
  }

  view_->SetWebContents(nullptr);
  view_ = nullptr;

  auto web_contents = web_contents_->GetWeakPtr();

  preview_zoom_controller_->ResetZoomForActivation();

  TabHelpers::AttachTabHelpers(web_contents_.get());

  // TODO(b:314242439): Should be called before the AttachTabHelpers() above.
  // We should update the preview mode status so that the AttachTabHelpers() can
  // know the helpers should be initialized for normal mode rather than preview
  // mode.
  web_contents_->WillActivatePreviewPage();

  // Detach WebContentsDelegate before passing WebContents to another
  // WebContentsDelegate. It would be not necessary, but it's natural because
  // the other paths do, e.g. TabDragController::DetachAndAttachToNewContext,
  // which moves a tab from Browser to another Browser.
  web_contents_->SetDelegate(nullptr);

  // Pass WebContents to Browser.
  WebContentsDelegate* delegate = initiator_web_contents.GetDelegate();
  CHECK(delegate);
  blink::mojom::WindowFeaturesPtr window_features =
      blink::mojom::WindowFeatures::New();
  delegate->AddNewContents(/*source*/ nullptr,
                           /*new_contents*/ std::move(web_contents_),
                           /*target_url*/ url_,
                           WindowOpenDisposition::NEW_FOREGROUND_TAB,
                           *window_features,
                           /*user_gesture*/ true,
                           /*was_blocked*/ nullptr);

  Activate(web_contents);
}

void PreviewTab::Activate(base::WeakPtr<content::WebContents> web_contents) {
  CHECK(web_contents);
  web_contents->ActivatePreviewPage();
}

// Copied from chrome/browser/ui/views/accelerator_table.h
struct AcceleratorMapping {
  ui::KeyboardCode keycode;
  int modifiers;
  int command_id;
};

constexpr AcceleratorMapping kAcceleratorMap[] = {
    {ui::VKEY_OEM_MINUS, ui::EF_PLATFORM_ACCELERATOR, IDC_ZOOM_MINUS},
    {ui::VKEY_SUBTRACT, ui::EF_PLATFORM_ACCELERATOR, IDC_ZOOM_MINUS},
    {ui::VKEY_0, ui::EF_PLATFORM_ACCELERATOR, IDC_ZOOM_NORMAL},
    {ui::VKEY_NUMPAD0, ui::EF_PLATFORM_ACCELERATOR, IDC_ZOOM_NORMAL},
    {ui::VKEY_OEM_PLUS, ui::EF_PLATFORM_ACCELERATOR, IDC_ZOOM_PLUS},
    {ui::VKEY_ADD, ui::EF_PLATFORM_ACCELERATOR, IDC_ZOOM_PLUS},
};

void PreviewTab::RegisterKeyboardAccelerators() {
  for (const auto& entry : kAcceleratorMap) {
    ui::Accelerator accelerator(entry.keycode, entry.modifiers);
    accelerator_table_[accelerator] = entry.command_id;
    view_->GetFocusManager()->RegisterAccelerator(
        accelerator, ui::AcceleratorManager::HandlerPriority::kNormalPriority,
        this);
  }
}

bool PreviewTab::CanHandleAccelerators() const {
  return web_contents_ != nullptr;
}

bool PreviewTab::AcceleratorPressed(const ui::Accelerator& accelerator) {
  auto it = accelerator_table_.find(accelerator);
  if (it == accelerator_table_.end()) {
    return false;
  }

  switch (it->second) {
    case IDC_ZOOM_MINUS:
      preview_zoom_controller_->Zoom(content::PAGE_ZOOM_OUT);
      break;
    case IDC_ZOOM_NORMAL:
      preview_zoom_controller_->Zoom(content::PAGE_ZOOM_RESET);
      break;
    case IDC_ZOOM_PLUS:
      preview_zoom_controller_->Zoom(content::PAGE_ZOOM_IN);
      break;
    default:
      NOTREACHED();
  }

  return true;
}

void PreviewTab::OnWidgetActivationChanged(views::Widget* widget, bool active) {
  if (!active) {
    preview_manager_->Cancel(content::PreviewCancelReason::Build(
        content::PreviewFinalStatus::kCancelledByWindowClose));
  }
}
