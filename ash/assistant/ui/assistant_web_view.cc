// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/assistant_web_view.h"

#include <algorithm>
#include <utility>

#include "ash/assistant/assistant_controller.h"
#include "ash/assistant/assistant_ui_controller.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/util/deep_link_util.h"
#include "ash/public/cpp/app_list/answer_card_contents_registry.h"
#include "ash/public/interfaces/web_contents_manager.mojom.h"
#include "base/callback.h"
#include "base/unguessable_token.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/canvas.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/painter.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// ContentViewMaskPainter -------------------------------------------------

class ContentViewMaskPainter : public views::Painter {
 public:
  ContentViewMaskPainter() = default;
  ~ContentViewMaskPainter() override = default;

  // views::Painter:
  gfx::Size GetMinimumSize() const override { return gfx::Size(); }

  void Paint(gfx::Canvas* canvas, const gfx::Size& size) override {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(SK_ColorBLACK);

    SkRRect rect;
    rect.setRectRadii(
        SkRect::MakeWH(size.width(), size.height()),
        (const SkVector[]){
            /*upper_left=*/SkVector::Make(0, 0),
            /*upper_right=*/SkVector::Make(0, 0),
            /*lower_right=*/SkVector::Make(kCornerRadiusDip, kCornerRadiusDip),
            /*lower_left=*/SkVector::Make(kCornerRadiusDip, kCornerRadiusDip)});

    canvas->sk_canvas()->drawRRect(rect, flags);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ContentViewMaskPainter);
};

}  // namespace

// AssistantWebView ------------------------------------------------------------

AssistantWebView::AssistantWebView(AssistantController* assistant_controller)
    : assistant_controller_(assistant_controller),
      web_contents_request_factory_(this) {
  InitLayout();

  assistant_controller_->AddObserver(this);
}

AssistantWebView::~AssistantWebView() {
  assistant_controller_->RemoveObserver(this);
  ReleaseWebContents();
}

const char* AssistantWebView::GetClassName() const {
  return "AssistantWebView";
}

gfx::Size AssistantWebView::CalculatePreferredSize() const {
  return gfx::Size(kPreferredWidthDip, GetHeightForWidth(kPreferredWidthDip));
}

int AssistantWebView::GetHeightForWidth(int width) const {
  // |height| <= |kMaxHeightDip|.
  // |height| should not exceed the height of the usable work area.
  gfx::Rect usable_work_area =
      assistant_controller_->ui_controller()->model()->usable_work_area();

  return std::min(kMaxHeightDip, usable_work_area.height());
}

void AssistantWebView::ChildPreferredSizeChanged(views::View* child) {
  // Because AssistantWebView has a fixed size, it does not re-layout its
  // children when their preferred size changes. To address this, we need to
  // explicitly request a layout pass.
  Layout();
  SchedulePaint();
}

void AssistantWebView::OnViewIsDeleting(views::View* view) {
  DCHECK_EQ(content_view_, view);

  // It's possible for |content_view_| to be deleted before AssistantWebView.
  // When this happens, we need to perform clean up on |content_view_| before
  // it is destroyed and clear references so that we don't try to perform
  // clean up on the destroyed instance when destroying AssistantWebView.
  content_view_->RemoveObserver(this);
  content_view_ = nullptr;
}

void AssistantWebView::OnWindowBoundsChanged(aura::Window* window,
                                             const gfx::Rect& old_bounds,
                                             const gfx::Rect& new_bounds,
                                             ui::PropertyChangeReason reason) {
  DCHECK_EQ(native_content_view_, window);

  // The mask layer should always match the bounds of the native content view.
  content_view_mask_->layer()->SetBounds(gfx::Rect(new_bounds.size()));
}

void AssistantWebView::OnWindowDestroying(aura::Window* window) {
  DCHECK_EQ(native_content_view_, window);

  // It's possible for |native_content_view_| to be deleted before
  // AssistantWebView. When this happens, we need to perform clean up on
  // |native_content_view_| before it is destroyed and clear references so that
  // we don't try to perform clean up on the destroyed instance when destroying
  // AssistantWebView.
  native_content_view_->RemoveObserver(this);
  native_content_view_->layer()->SetMaskLayer(nullptr);
  native_content_view_ = nullptr;
}

void AssistantWebView::InitLayout() {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  // Caption bar.
  caption_bar_ = new CaptionBar();
  caption_bar_->set_delegate(this);
  caption_bar_->SetButtonVisible(CaptionButtonId::kMinimize, false);
  AddChildView(caption_bar_);

  // Content mask.
  // This is used to enforce corner radius on the contents' layer.
  content_view_mask_ = views::Painter::CreatePaintedLayer(
      std::make_unique<ContentViewMaskPainter>());
  content_view_mask_->layer()->SetFillsBoundsOpaquely(false);
}

bool AssistantWebView::OnCaptionButtonPressed(CaptionButtonId id) {
  CaptionBarDelegate* delegate = assistant_controller_->ui_controller();

  // We need special handling of the back button. When possible, the back button
  // should navigate backwards in the managed WebContents' history stack.
  if (id == CaptionButtonId::kBack && web_contents_id_token_.has_value()) {
    assistant_controller_->NavigateWebContentsBack(
        web_contents_id_token_.value(),
        base::BindOnce(
            [](CaptionBarDelegate* delegate, bool navigated) {
              // If the WebContents' did not navigate it is because we are
              // already at the first entry in the history stack and we cannot
              // navigate back any further. In this case, we give back control
              // to our primary caption button delegate.
              if (!navigated)
                delegate->OnCaptionButtonPressed(CaptionButtonId::kBack);
            },
            base::Unretained(delegate)));
    return true;
  }

  // For all other buttons we defer to our primary caption button delegate.
  return delegate->OnCaptionButtonPressed(id);
}

void AssistantWebView::OnDeepLinkReceived(
    assistant::util::DeepLinkType type,
    const std::map<std::string, std::string>& params) {
  if (!assistant::util::IsWebDeepLinkType(type))
    return;

  ReleaseWebContents();

  web_contents_id_token_ = base::UnguessableToken::Create();

  gfx::Size preferred_size_dip =
      gfx::Size(kPreferredWidthDip,
                kMaxHeightDip - caption_bar_->GetPreferredSize().height());

  ash::mojom::ManagedWebContentsParamsPtr web_contents_params(
      ash::mojom::ManagedWebContentsParams::New());
  web_contents_params->url = GetWebUrl(type).value();
  web_contents_params->min_size_dip = preferred_size_dip;
  web_contents_params->max_size_dip = preferred_size_dip;

  assistant_controller_->ManageWebContents(
      web_contents_id_token_.value(), std::move(web_contents_params),
      base::BindOnce(&AssistantWebView::OnWebContentsReady,
                     web_contents_request_factory_.GetWeakPtr()));
}

void AssistantWebView::OnWebContentsReady(
    const base::Optional<base::UnguessableToken>& embed_token) {
  // TODO(dmblack): In the event that rendering fails, we should show a useful
  // message to the user (and perhaps close the UI).
  if (!embed_token.has_value())
    return;

  // When web contents are rendered in process, the WebView associated with the
  // returned |embed_token| is found in the AnswerCardContentsRegistry.
  if (app_list::AnswerCardContentsRegistry::Get()) {
    content_view_ = app_list::AnswerCardContentsRegistry::Get()->GetView(
        embed_token.value());
    content_view_->AddObserver(this);

    // Cache a reference to the content's native view and observe it so that
    // we can monitor changes to bounds and lifecycle.
    native_content_view_ =
        app_list::AnswerCardContentsRegistry::Get()->GetNativeView(
            embed_token.value());
    native_content_view_->AddObserver(this);

    // Apply a mask layer to enforce corner radius. The mask bounds must always
    // match the bounds of |native_content_view_|. We sync bounds prior to
    // applying the mask to prevent a DCHECK failure in cc::Layer.
    content_view_mask_->layer()->SetBounds(
        gfx::Rect(native_content_view_->GetBoundsInScreen().size()));
    native_content_view_->layer()->SetMaskLayer(content_view_mask_->layer());

    AddChildView(content_view_);
  }

  // TODO(dmblack): Handle mash case.
}

void AssistantWebView::ReleaseWebContents() {
  web_contents_request_factory_.InvalidateWeakPtrs();

  if (!web_contents_id_token_.has_value())
    return;

  if (content_view_) {
    content_view_->RemoveObserver(this);
    RemoveChildView(content_view_);

    // In Mash, |content_view_| was owned by the view hierarchy prior to its
    // removal. Otherwise the view is owned by the WebContentsManager.
    if (!content_view_->owned_by_client())
      delete content_view_;

    content_view_ = nullptr;
  }

  if (native_content_view_) {
    native_content_view_->RemoveObserver(this);
    native_content_view_->layer()->SetMaskLayer(nullptr);
    native_content_view_ = nullptr;
  }

  assistant_controller_->ReleaseWebContents(web_contents_id_token_.value());
  web_contents_id_token_.reset();
}

}  // namespace ash
