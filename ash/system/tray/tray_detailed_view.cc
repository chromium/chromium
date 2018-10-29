// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_detailed_view.h"

#include "ash/public/cpp/ash_view_ids.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/tray/detailed_view_delegate.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/system_menu_button.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_item.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_item_style.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tri_view.h"
#include "base/containers/adapters.h"
#include "third_party/skia/include/core/SkDrawLooper.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/clip_recorder.h"
#include "ui/compositor/paint_context.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view_targeter.h"
#include "ui/views/view_targeter_delegate.h"

namespace ash {
namespace {

// The index of the horizontal rule below the title row.
const int kTitleRowSeparatorIndex = 1;

// A view that is used as ScrollView contents. It supports designating some of
// the children as sticky header rows. The sticky header rows are not scrolled
// above the top of the visible viewport until the next one "pushes" it up and
// are painted above other children. To indicate that a child is a sticky header
// row use set_id(VIEW_ID_STICKY_HEADER).
class ScrollContentsView : public views::View {
 public:
  explicit ScrollContentsView(DetailedViewDelegate* delegate)
      : delegate_(delegate) {
    box_layout_ = SetLayoutManager(
        std::make_unique<views::BoxLayout>(views::BoxLayout::kVertical));
  }
  ~ScrollContentsView() override = default;

 protected:
  // views::View:
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override {
    PositionHeaderRows();
  }

  void PaintChildren(const views::PaintInfo& paint_info) override {
    int sticky_header_height = 0;
    for (const auto& header : headers_) {
      // Sticky header is at the top.
      if (header.view->y() != header.natural_offset) {
        sticky_header_height = header.view->bounds().height();
        DCHECK_EQ(VIEW_ID_STICKY_HEADER, header.view->id());
        break;
      }
    }
    // Paint contents other than sticky headers. If sticky header is at the top,
    // it clips the header's height so that nothing is shown behind the header.
    {
      ui::ClipRecorder clip_recorder(paint_info.context());
      gfx::Rect clip_rect = gfx::Rect(paint_info.paint_recording_size()) -
                            paint_info.offset_from_parent();
      gfx::Insets clip_insets(sticky_header_height, 0, 0, 0);
      clip_rect.Inset(clip_insets.Scale(paint_info.paint_recording_scale_x(),
                                        paint_info.paint_recording_scale_y()));
      clip_recorder.ClipRect(clip_rect);
      for (int i = 0; i < child_count(); ++i) {
        auto* child = child_at(i);
        if (child->id() != VIEW_ID_STICKY_HEADER && !child->layer())
          child->Paint(paint_info);
      }
    }
    // Paint sticky headers.
    for (int i = 0; i < child_count(); ++i) {
      auto* child = child_at(i);
      if (child->id() == VIEW_ID_STICKY_HEADER && !child->layer())
        child->Paint(paint_info);
    }

    bool did_draw_shadow = false;
    // Paint header row separators.
    for (auto& header : headers_)
      did_draw_shadow =
          PaintDelineation(header, paint_info.context()) || did_draw_shadow;

    // Draw a shadow at the top of the viewport when scrolled, but only if a
    // header didn't already draw one. Overlap the shadow with the separator
    // that's below the header view so we don't get both a separator and a full
    // shadow.
    if (y() != 0 && !did_draw_shadow)
      DrawShadow(
          paint_info.context(),
          gfx::Rect(0, 0, width(), -y() - TrayConstants::separator_width()));
  }

  void Layout() override {
    views::View::Layout();
    headers_.clear();
    for (int i = 0; i < child_count(); ++i) {
      views::View* view = child_at(i);
      if (view->id() == VIEW_ID_STICKY_HEADER)
        headers_.emplace_back(view);
    }
    PositionHeaderRows();
  }

  View::Views GetChildrenInZOrder() override {
    View::Views children;
    // Iterate over regular children and later over the sticky headers to keep
    // the sticky headers above in Z-order.
    for (int i = 0; i < child_count(); ++i) {
      if (child_at(i)->id() != VIEW_ID_STICKY_HEADER)
        children.push_back(child_at(i));
    }
    for (int i = 0; i < child_count(); ++i) {
      if (child_at(i)->id() == VIEW_ID_STICKY_HEADER)
        children.push_back(child_at(i));
    }
    DCHECK_EQ(child_count(), static_cast<int>(children.size()));
    return children;
  }

  void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) override {
    if (!details.is_add && details.parent == this) {
      headers_.erase(std::remove_if(headers_.begin(), headers_.end(),
                                    [details](const Header& header) {
                                      return header.view == details.child;
                                    }),
                     headers_.end());
    } else if (details.is_add && details.parent == this &&
               details.child == child_at(0)) {
      // We always want padding on the bottom of the scroll contents.
      // We only want padding on the top of the scroll contents if the first
      // child is not a header (in that case, the padding is built into the
      // header).
      DCHECK_EQ(box_layout_, GetLayoutManager());
      box_layout_->set_inside_border_insets(
          gfx::Insets(details.child->id() == VIEW_ID_STICKY_HEADER
                          ? 0
                          : kMenuSeparatorVerticalPadding,
                      0, kMenuSeparatorVerticalPadding, 0));
    }
  }

 private:
  const int kShadowOffsetY = 2;
  const int kShadowBlur = 2;

  // A structure that keeps the original offset of each header between the
  // calls to Layout() to allow keeping track of which view should be sticky.
  struct Header {
    explicit Header(views::View* view)
        : view(view), natural_offset(view->y()), draw_separator_below(false) {}

    // A header View that can be decorated as sticky.
    views::View* view;

    // Offset from the top of ScrollContentsView to |view|'s original vertical
    // position.
    int natural_offset;

    // True when a separator needs to be painted below the header when another
    // header is pushing |this| header up.
    bool draw_separator_below;
  };

  // Adjusts y-position of header rows allowing one or two rows to stick to the
  // top of the visible viewport.
  void PositionHeaderRows() {
    const int scroll_offset = -y();
    Header* previous_header = nullptr;
    for (auto& header : base::Reversed(headers_)) {
      views::View* header_view = header.view;
      bool draw_separator_below = false;
      if (header.natural_offset >= scroll_offset) {
        previous_header = &header;
        header_view->SetY(header.natural_offset);
      } else {
        if (previous_header && previous_header->view->y() <=
                                   scroll_offset + header_view->height()) {
          // Lower header displacing the header above.
          draw_separator_below = true;
          header_view->SetY(previous_header->view->y() - header_view->height());
        } else {
          // A header becomes sticky.
          header_view->SetY(scroll_offset);
          header_view->Layout();
          header_view->SchedulePaint();
        }
      }
      if (header.draw_separator_below != draw_separator_below) {
        header.draw_separator_below = draw_separator_below;
        delegate_->ShowStickyHeaderSeparator(header_view, draw_separator_below);
      }
      if (header.natural_offset < scroll_offset)
        break;
    }
  }

  // Paints a separator for a header view. The separator can be a horizontal
  // rule or a horizontal shadow, depending on whether the header is sticking to
  // the top of the scroll viewport. The return value indicates whether a shadow
  // was drawn.
  bool PaintDelineation(const Header& header, const ui::PaintContext& context) {
    const View* view = header.view;

    // If the header is where it normally belongs or If the header is pushed by
    // a header directly below it, draw nothing.
    if (view->y() == header.natural_offset || header.draw_separator_below)
      return false;

    // Otherwise, draw a shadow below.
    DrawShadow(context,
               gfx::Rect(0, 0, view->width(), view->bounds().bottom()));
    return true;
  }

  // Draws a drop shadow below |shadowed_area|.
  void DrawShadow(const ui::PaintContext& context,
                  const gfx::Rect& shadowed_area) {
    ui::PaintRecorder recorder(context, size());
    gfx::Canvas* canvas = recorder.canvas();
    cc::PaintFlags flags;
    gfx::ShadowValues shadow;
    shadow.emplace_back(gfx::Vector2d(0, kShadowOffsetY), kShadowBlur,
                        kMenuSeparatorColor);
    flags.setLooper(gfx::CreateShadowDrawLooper(shadow));
    flags.setAntiAlias(true);
    canvas->ClipRect(shadowed_area, SkClipOp::kDifference);
    canvas->DrawRect(shadowed_area, flags);
  }

  DetailedViewDelegate* const delegate_;

  views::BoxLayout* box_layout_ = nullptr;

  // Header child views that stick to the top of visible viewport when scrolled.
  std::vector<Header> headers_;

  DISALLOW_COPY_AND_ASSIGN(ScrollContentsView);
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// TrayDetailedView:

TrayDetailedView::TrayDetailedView(DetailedViewDelegate* delegate)
    : delegate_(delegate),
      box_layout_(nullptr),
      scroller_(nullptr),
      scroll_content_(nullptr),
      progress_bar_(nullptr),
      tri_view_(nullptr),
      back_button_(nullptr) {
  box_layout_ = SetLayoutManager(
      std::make_unique<views::BoxLayout>(views::BoxLayout::kVertical));
  SetBackground(views::CreateSolidBackground(
      delegate_->GetBackgroundColor(GetNativeTheme())));
}

TrayDetailedView::~TrayDetailedView() = default;

void TrayDetailedView::OnViewClicked(views::View* sender) {
  HandleViewClicked(sender);
}

void TrayDetailedView::ButtonPressed(views::Button* sender,
                                     const ui::Event& event) {
  if (sender == back_button_) {
    TransitionToMainView();
    return;
  }

  HandleButtonPressed(sender, event);
}

void TrayDetailedView::CreateTitleRow(int string_id) {
  DCHECK(!tri_view_);

  tri_view_ = delegate_->CreateTitleRow(string_id);

  back_button_ = delegate_->CreateBackButton(this);
  tri_view_->AddView(TriView::Container::START, back_button_);

  AddChildViewAt(tri_view_, 0);
  AddChildViewAt(delegate_->CreateTitleSeparator(), kTitleRowSeparatorIndex);

  CreateExtraTitleRowButtons();
  Layout();
}

void TrayDetailedView::CreateScrollableList() {
  DCHECK(!scroller_);
  scroll_content_ = new ScrollContentsView(delegate_);
  scroller_ = new views::ScrollView;
  scroller_->set_draw_overflow_indicator(
      delegate_->IsOverflowIndicatorEnabled());
  scroller_->SetContents(scroll_content_);
  // TODO(varkha): Make the sticky rows work with EnableViewPortLayer().
  scroller_->SetBackgroundColor(
      delegate_->GetBackgroundColor(GetNativeTheme()));

  AddChildView(scroller_);
  box_layout_->SetFlexForView(scroller_, 1);
}

HoverHighlightView* TrayDetailedView::AddScrollListItem(
    const gfx::VectorIcon& icon,
    const base::string16& text) {
  HoverHighlightView* item = delegate_->CreateScrollListItem(this, icon, text);
  scroll_content_->AddChildView(item);
  return item;
}

HoverHighlightView* TrayDetailedView::AddScrollListCheckableItem(
    const gfx::VectorIcon& icon,
    const base::string16& text,
    bool checked) {
  HoverHighlightView* item = AddScrollListItem(icon, text);
  TrayPopupUtils::InitializeAsCheckableRow(item, checked);
  return item;
}

HoverHighlightView* TrayDetailedView::AddScrollListCheckableItem(
    const base::string16& text,
    bool checked) {
  return AddScrollListCheckableItem(gfx::kNoneIcon, text, checked);
}

void TrayDetailedView::SetupConnectedScrollListItem(HoverHighlightView* view) {
  DCHECK(view->is_populated());

  view->SetSubText(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NETWORK_STATUS_CONNECTED));
  TrayPopupItemStyle style(TrayPopupItemStyle::FontStyle::CAPTION);
  style.set_color_style(TrayPopupItemStyle::ColorStyle::CONNECTED);
  style.SetupLabel(view->sub_text_label());
}

void TrayDetailedView::SetupConnectingScrollListItem(HoverHighlightView* view) {
  DCHECK(view->is_populated());

  view->SetSubText(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NETWORK_STATUS_CONNECTING));
}

TriView* TrayDetailedView::AddScrollListSubHeader(const gfx::VectorIcon& icon,
                                                  int text_id) {
  TriView* header = TrayPopupUtils::CreateSubHeaderRowView(true);
  TrayPopupUtils::ConfigureAsStickyHeader(header);

  views::Label* label = TrayPopupUtils::CreateDefaultLabel();
  label->SetText(l10n_util::GetStringUTF16(text_id));
  TrayPopupItemStyle style(TrayPopupItemStyle::FontStyle::SUB_HEADER);
  style.SetupLabel(label);
  header->AddView(TriView::Container::CENTER, label);

  views::ImageView* image_view = TrayPopupUtils::CreateMainImageView();
  image_view->SetImage(gfx::CreateVectorIcon(
      icon, GetNativeTheme()->GetSystemColor(
                ui::NativeTheme::kColorId_ProminentButtonColor)));
  header->AddView(TriView::Container::START, image_view);

  scroll_content_->AddChildView(header);
  return header;
}

TriView* TrayDetailedView::AddScrollListSubHeader(int text_id) {
  return AddScrollListSubHeader(gfx::kNoneIcon, text_id);
}

void TrayDetailedView::Reset() {
  RemoveAllChildViews(true);
  scroller_ = nullptr;
  scroll_content_ = nullptr;
  progress_bar_ = nullptr;
  back_button_ = nullptr;
  tri_view_ = nullptr;
}

void TrayDetailedView::ShowProgress(double value, bool visible) {
  DCHECK(tri_view_);
  if (!progress_bar_) {
    progress_bar_ = new views::ProgressBar(kTitleRowProgressBarHeight);
    progress_bar_->SetVisible(false);
    AddChildViewAt(progress_bar_, kTitleRowSeparatorIndex + 1);
  }

  progress_bar_->SetValue(value);
  progress_bar_->SetVisible(visible);
  child_at(kTitleRowSeparatorIndex)->SetVisible(!visible);
}

views::Button* TrayDetailedView::CreateInfoButton(int info_accessible_name_id) {
  return delegate_->CreateInfoButton(this, info_accessible_name_id);
}

views::Button* TrayDetailedView::CreateSettingsButton(
    int setting_accessible_name_id) {
  return delegate_->CreateSettingsButton(this, setting_accessible_name_id);
}

views::Button* TrayDetailedView::CreateHelpButton() {
  return delegate_->CreateHelpButton(this);
}

views::Separator* TrayDetailedView::CreateListSubHeaderSeparator() {
  return delegate_->CreateListSubHeaderSeparator();
}

void TrayDetailedView::HandleViewClicked(views::View* view) {
  NOTREACHED();
}

void TrayDetailedView::HandleButtonPressed(views::Button* sender,
                                           const ui::Event& event) {
  NOTREACHED();
}

void TrayDetailedView::CreateExtraTitleRowButtons() {}

void TrayDetailedView::TransitionToMainView() {
  delegate_->TransitionToMainView(back_button_ && back_button_->HasFocus());
}

void TrayDetailedView::CloseBubble() {
  delegate_->CloseBubble();
}

void TrayDetailedView::Layout() {
  views::View::Layout();
  if (scroller_ && !scroller_->is_bounded())
    scroller_->ClipHeightTo(0, scroller_->height());
}

int TrayDetailedView::GetHeightForWidth(int width) const {
  if (bounds().IsEmpty())
    return views::View::GetHeightForWidth(width);

  // The height of the bubble that contains this detailed view is set to
  // the preferred height of the default view, and that determines the
  // initial height of |this|. Always request to stay the same height.
  return height();
}

}  // namespace ash
