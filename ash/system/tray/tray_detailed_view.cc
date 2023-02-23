// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_detailed_view.h"

#include <cstring>
#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/controls/rounded_scroll_bar.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/tray/detailed_view_delegate.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/system_menu_button.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tri_view.h"
#include "base/check.h"
#include "base/containers/adapters.h"
#include "base/functional/bind.h"
#include "third_party/skia/include/core/SkDrawLooper.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/clip_recorder.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/paint_context.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_targeter.h"
#include "ui/views/view_targeter_delegate.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

// The index of the horizontal rule below the title row.
const int kTitleRowSeparatorIndex = 1;

constexpr int kQsItemBetweenSpacing = 8;

constexpr int kQsScrollViewCornerRadius = 16;

// Inset the scroll bar to avoid the rounded corners at top and bottom.
constexpr auto kQsScrollBarInsets =
    gfx::Insets::VH(kQsScrollViewCornerRadius, 0);

// Configures the TriView used for the title in a detailed view.
void ConfigureTitleTriView(TriView* tri_view, TriView::Container container) {
  std::unique_ptr<views::BoxLayout> layout;

  switch (container) {
    case TriView::Container::START:
    case TriView::Container::END: {
      const int left_padding = container == TriView::Container::START
                                   ? kUnifiedBackButtonLeftPadding
                                   : 0;
      layout = std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          gfx::Insets::TLBR(0, left_padding, 0, 0),
          features::IsQsRevampEnabled() ? kQsItemBetweenSpacing
                                        : kUnifiedTopShortcutSpacing);
      layout->set_main_axis_alignment(
          views::BoxLayout::MainAxisAlignment::kCenter);
      layout->set_cross_axis_alignment(
          views::BoxLayout::CrossAxisAlignment::kCenter);
      break;
    }
    case TriView::Container::CENTER:
      tri_view->SetFlexForContainer(TriView::Container::CENTER, 1.f);

      layout = std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical);
      layout->set_main_axis_alignment(
          views::BoxLayout::MainAxisAlignment::kCenter);
      if (features::IsQsRevampEnabled()) {
        layout->set_cross_axis_alignment(
            views::BoxLayout::CrossAxisAlignment::kCenter);
        break;
      }
      layout->set_cross_axis_alignment(
          views::BoxLayout::CrossAxisAlignment::kStretch);
      break;
  }

  tri_view->SetContainerLayout(container, std::move(layout));
  tri_view->SetMinSize(container,
                       gfx::Size(0, kUnifiedDetailedViewTitleRowHeight));
}

// A view that is used as ScrollView contents. It supports designating some of
// the children as sticky header rows. The sticky header rows are not scrolled
// above the top of the visible viewport until the next one "pushes" it up and
// are painted above other children. To indicate that a child is a sticky header
// row use SetID(VIEW_ID_STICKY_HEADER).
class ScrollContentsView : public views::View {
 public:
  ScrollContentsView() {
    box_layout_ = SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical));
    // NOTE: Pre-QsRevamp, insets are added in ViewHierarchyChanged().
  }

  ScrollContentsView(const ScrollContentsView&) = delete;
  ScrollContentsView& operator=(const ScrollContentsView&) = delete;

  ~ScrollContentsView() override = default;

 protected:
  // views::View:
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override {
    PositionHeaderRows();
  }

  void PaintChildren(const views::PaintInfo& paint_info) override {
    // No sticky header and no shadow for the revamped view.
    if (features::IsQsRevampEnabled()) {
      views::View::PaintChildren(paint_info);
      return;
    }

    int sticky_header_height = 0;
    for (const auto& header : headers_) {
      // Sticky header is at the top.
      if (header.view->y() != header.natural_offset) {
        sticky_header_height = header.view->bounds().height();
        DCHECK_EQ(VIEW_ID_STICKY_HEADER, header.view->GetID());
        break;
      }
    }
    // Paint contents other than sticky headers. If sticky header is at the top,
    // it clips the header's height so that nothing is shown behind the header.
    {
      ui::ClipRecorder clip_recorder(paint_info.context());
      gfx::Rect clip_rect = gfx::Rect(paint_info.paint_recording_size()) -
                            paint_info.offset_from_parent();
      auto clip_insets = gfx::Insets::TLBR(sticky_header_height, 0, 0, 0);
      clip_rect.Inset(gfx::ScaleToFlooredInsets(
          clip_insets, paint_info.paint_recording_scale_x(),
          paint_info.paint_recording_scale_y()));
      clip_recorder.ClipRect(clip_rect);
      for (auto* child : children()) {
        if (child->GetID() != VIEW_ID_STICKY_HEADER && !child->layer()) {
          child->Paint(paint_info);
        }
      }
    }
    // Paint sticky headers.
    for (auto* child : children()) {
      if (child->GetID() == VIEW_ID_STICKY_HEADER && !child->layer()) {
        child->Paint(paint_info);
      }
    }

    bool did_draw_shadow = false;
    // Paint header row separators.
    for (auto& header : headers_) {
      did_draw_shadow =
          PaintDelineation(header, paint_info.context()) || did_draw_shadow;
    }

    // Draw a shadow at the top of the viewport when scrolled, but only if a
    // header didn't already draw one. Overlap the shadow with the separator
    // that's below the header view so we don't get both a separator and a full
    // shadow.
    if (y() != 0 && !did_draw_shadow) {
      DrawShadow(paint_info.context(),
                 gfx::Rect(0, 0, width(), -y() - kTraySeparatorWidth));
    }
  }

  void Layout() override {
    views::View::Layout();

    // No sticky headers for the revamped view.
    if (features::IsQsRevampEnabled()) {
      return;
    }

    headers_.clear();
    for (auto* child : children()) {
      if (child->GetID() == VIEW_ID_STICKY_HEADER) {
        headers_.emplace_back(child);
      }
    }
    PositionHeaderRows();
  }

  const char* GetClassName() const override { return "ScrollContentsView"; }

  View::Views GetChildrenInZOrder() override {
    // Place sticky headers last in the child order so that they wind up on top
    // in Z order.
    View::Views children_in_z_order = children();
    std::stable_partition(children_in_z_order.begin(),
                          children_in_z_order.end(), [](const View* child) {
                            return child->GetID() != VIEW_ID_STICKY_HEADER;
                          });
    return children_in_z_order;
  }

  void ViewHierarchyChanged(
      const views::ViewHierarchyChangedDetails& details) override {
    // No sticky headers or border insets in the revamped view.
    if (features::IsQsRevampEnabled()) {
      return;
    }

    if (!details.is_add && details.parent == this) {
      headers_.erase(std::remove_if(headers_.begin(), headers_.end(),
                                    [details](const Header& header) {
                                      return header.view == details.child;
                                    }),
                     headers_.end());
    } else if (details.is_add && details.parent == this &&
               details.child == children().front()) {
      // We always want padding on the bottom of the scroll contents.
      // We only want padding on the top of the scroll contents if the first
      // child is not a header (in that case, the padding is built into the
      // header).
      DCHECK_EQ(box_layout_, GetLayoutManager());
      box_layout_->set_inside_border_insets(
          gfx::Insets::TLBR(details.child->GetID() == VIEW_ID_STICKY_HEADER
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
        ShowStickyHeaderSeparator(header_view, draw_separator_below);
      }
      if (header.natural_offset < scroll_offset) {
        break;
      }
    }
  }

  // Configures `view` to have a visible separator below.
  void ShowStickyHeaderSeparator(views::View* view, bool show_separator) {
    if (show_separator) {
      view->SetBorder(views::CreatePaddedBorder(
          views::CreateSolidSidedBorder(
              gfx::Insets::TLBR(0, 0, kTraySeparatorWidth, 0),
              AshColorProvider::Get()->GetContentLayerColor(
                  AshColorProvider::ContentLayerType::kSeparatorColor)),
          gfx::Insets::TLBR(kMenuSeparatorVerticalPadding, 0,
                            kMenuSeparatorVerticalPadding - kTraySeparatorWidth,
                            0)));
    } else {
      view->SetBorder(views::CreateEmptyBorder(
          gfx::Insets::VH(kMenuSeparatorVerticalPadding, 0)));
    }
    view->SchedulePaint();
  }

  // Paints a separator for a header view. The separator can be a horizontal
  // rule or a horizontal shadow, depending on whether the header is sticking to
  // the top of the scroll viewport. The return value indicates whether a shadow
  // was drawn.
  bool PaintDelineation(const Header& header, const ui::PaintContext& context) {
    const View* view = header.view;

    // If the header is where it normally belongs or If the header is pushed by
    // a header directly below it, draw nothing.
    if (view->y() == header.natural_offset || header.draw_separator_below) {
      return false;
    }

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
    shadow.emplace_back(
        gfx::Vector2d(0, kShadowOffsetY), kShadowBlur,
        AshColorProvider::Get()->GetContentLayerColor(
            AshColorProvider::ContentLayerType::kSeparatorColor));
    flags.setLooper(gfx::CreateShadowDrawLooper(shadow));
    flags.setAntiAlias(true);
    canvas->ClipRect(shadowed_area, SkClipOp::kDifference);
    canvas->DrawRect(shadowed_area, flags);
  }

  views::BoxLayout* box_layout_ = nullptr;

  // Header child views that stick to the top of visible viewport when scrolled.
  std::vector<Header> headers_;
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// TrayDetailedView:

TrayDetailedView::TrayDetailedView(DetailedViewDelegate* delegate)
    : delegate_(delegate) {
  box_layout_ = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  if (features::IsQsRevampEnabled()) {
    IgnoreSeparator();
  }
}

TrayDetailedView::~TrayDetailedView() = default;

void TrayDetailedView::OnViewClicked(views::View* sender) {
  DCHECK(sender);
  HandleViewClicked(sender);
}

void TrayDetailedView::OverrideProgressBarAccessibleName(
    const std::u16string& name) {
  progress_bar_accessible_name_ = name;
}

void TrayDetailedView::CreateTitleRow(int string_id, bool create_back_button) {
  DCHECK(!tri_view_);

  tri_view_ = AddChildViewAt(CreateTitleTriView(string_id), 0);
  if (create_back_button) {
    back_button_ = delegate_->CreateBackButton(base::BindRepeating(
        &TrayDetailedView::TransitionToMainView, base::Unretained(this)));
    back_button_->SetID(VIEW_ID_QS_DETAILED_VIEW_BACK_BUTTON);
    tri_view_->AddView(TriView::Container::START, back_button_);
  }

  // If this view doesn't have a separator, adds an empty view as a placeholder
  // so that the views below won't move up when the `progress_bar_` becomes
  // invisible.
  if (!has_separator_) {
    auto buffer_view = std::make_unique<views::View>();
    buffer_view->SetPreferredSize(gfx::Size(1, kTitleRowProgressBarHeight));
    AddChildViewAt(std::move(buffer_view), kTitleRowSeparatorIndex);
  } else {
    title_separator_ =
        AddChildViewAt(CreateTitleSeparator(), kTitleRowSeparatorIndex);
  }

  CreateExtraTitleRowButtons();

  if (!features::IsQsRevampEnabled()) {
    Layout();
    return;
  }
  // Makes the `tri_view_`'s `START` and `END`container have the same width,
  // so the header text will be in the center of the `QuickSettingsView`
  // horizontally.
  auto* start_view =
      tri_view_->children()[static_cast<size_t>(TriView::Container::START)];
  auto* end_view =
      tri_view_->children()[static_cast<size_t>(TriView::Container::END)];
  int start_width = start_view->GetPreferredSize().width();
  int end_width = end_view->GetPreferredSize().width();
  if (start_width < end_width) {
    DCHECK(start_view->GetVisible());
    start_view->SetBorder(views::CreateEmptyBorder(
        gfx::Insets::TLBR(0, 0, 0, end_width - start_width)));
  } else {
    // Ensure the end container is visible, even if it has no buttons.
    tri_view_->SetContainerVisible(TriView::Container::END, true);
    end_view->SetBorder(views::CreateEmptyBorder(
        gfx::Insets::TLBR(0, start_width - end_width, 0, 0)));
  }

  Layout();
}

void TrayDetailedView::CreateScrollableList() {
  DCHECK(!scroller_);
  auto scroll_content = std::make_unique<ScrollContentsView>();
  scroller_ = AddChildView(std::make_unique<views::ScrollView>());
  scroller_->SetDrawOverflowIndicator(false);
  scroll_content_ = scroller_->SetContents(std::move(scroll_content));
  // TODO(varkha): Make the sticky rows work with EnableViewPortLayer().

  if (features::IsQsRevampEnabled()) {
    auto vertical_scroll = std::make_unique<RoundedScrollBar>(
        /*horizontal=*/false);
    vertical_scroll->SetInsets(kQsScrollBarInsets);
    scroller_->SetVerticalScrollBar(std::move(vertical_scroll));
    scroller_->SetProperty(views::kMarginsKey,
                           delegate_->GetScrollViewMargin());
    scroller_->SetPaintToLayer();
    scroller_->layer()->SetFillsBoundsOpaquely(false);
    scroller_->layer()->SetRoundedCornerRadius(
        gfx::RoundedCornersF(kQsScrollViewCornerRadius));
  }

  // Override the default theme-based color to remove the background.
  scroller_->SetBackgroundColor(absl::nullopt);

  box_layout_->SetFlexForView(scroller_, 1);
}

HoverHighlightView* TrayDetailedView::AddScrollListItem(
    views::View* container,
    const gfx::VectorIcon& icon,
    const std::u16string& text) {
  HoverHighlightView* item = container->AddChildView(
      std::make_unique<HoverHighlightView>(/*listener=*/this));
  if (icon.is_empty()) {
    item->AddLabelRow(text);
  } else {
    item->AddIconAndLabel(
        gfx::CreateVectorIcon(
            icon, AshColorProvider::Get()->GetContentLayerColor(
                      AshColorProvider::ContentLayerType::kIconColorPrimary)),
        text);
  }

  if (features::IsQsRevampEnabled()) {
    views::FocusRing::Install(item);
    views::InstallRoundRectHighlightPathGenerator(item, gfx::Insets(2),
                                                  /*corner_radius=*/0);
    // Unset the focus painter set by `ActionableView`.
    item->SetFocusPainter(nullptr);
  }

  return item;
}

HoverHighlightView* TrayDetailedView::AddScrollListCheckableItem(
    views::View* container,
    const gfx::VectorIcon& icon,
    const std::u16string& text,
    bool checked,
    bool enterprise_managed) {
  HoverHighlightView* item = AddScrollListItem(container, icon, text);
  if (enterprise_managed) {
    item->SetAccessibleName(l10n_util::GetStringFUTF16(
        IDS_ASH_ACCESSIBILITY_FEATURE_MANAGED, text));
  }
  TrayPopupUtils::InitializeAsCheckableRow(item, checked, enterprise_managed);
  return item;
}

TriView* TrayDetailedView::AddScrollListSubHeader(views::View* container,
                                                  const gfx::VectorIcon& icon,
                                                  int text_id) {
  TriView* header = TrayPopupUtils::CreateSubHeaderRowView(true);
  TrayPopupUtils::ConfigureAsStickyHeader(header);

  auto* color_provider = AshColorProvider::Get();
  sub_header_label_ = TrayPopupUtils::CreateDefaultLabel();
  sub_header_label_->SetText(l10n_util::GetStringUTF16(text_id));
  sub_header_label_->SetEnabledColor(color_provider->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary));
  TrayPopupUtils::SetLabelFontList(sub_header_label_,
                                   TrayPopupUtils::FontStyle::kSubHeader);
  header->AddView(TriView::Container::CENTER, sub_header_label_);

  sub_header_image_view_ =
      TrayPopupUtils::CreateMainImageView(/*use_wide_layout=*/false);
  sub_header_icon_ = &icon;
  sub_header_image_view_->SetImage(gfx::CreateVectorIcon(
      icon, color_provider->GetContentLayerColor(
                AshColorProvider::ContentLayerType::kIconColorPrimary)));
  header->AddView(TriView::Container::START, sub_header_image_view_);

  container->AddChildView(header);
  return header;
}

void TrayDetailedView::Reset() {
  RemoveAllChildViews();
  scroller_ = nullptr;
  scroll_content_ = nullptr;
  progress_bar_ = nullptr;
  back_button_ = nullptr;
  tri_view_ = nullptr;
  title_label_ = nullptr;
  sub_header_label_ = nullptr;
  sub_header_image_view_ = nullptr;
  title_separator_ = nullptr;
}

void TrayDetailedView::ShowProgress(double value, bool visible) {
  DCHECK(tri_view_);
  if (!progress_bar_) {
    progress_bar_ = AddChildViewAt(
        std::make_unique<views::ProgressBar>(kTitleRowProgressBarHeight),
        kTitleRowSeparatorIndex + 1);
    progress_bar_->GetViewAccessibility().OverrideName(
        progress_bar_accessible_name_.value_or(l10n_util::GetStringUTF16(
            IDS_ASH_STATUS_TRAY_PROGRESS_BAR_ACCESSIBLE_NAME)));
    progress_bar_->SetVisible(false);
    progress_bar_->SetForegroundColor(
        AshColorProvider::Get()->GetContentLayerColor(
            AshColorProvider::ContentLayerType::kIconColorProminent));
  }

  progress_bar_->SetValue(value);
  progress_bar_->SetVisible(visible);
  children()[size_t{kTitleRowSeparatorIndex}]->SetVisible(!visible);
}

views::Button* TrayDetailedView::CreateInfoButton(
    views::Button::PressedCallback callback,
    int info_accessible_name_id) {
  return delegate_->CreateInfoButton(std::move(callback),
                                     info_accessible_name_id);
}

views::Button* TrayDetailedView::CreateSettingsButton(
    views::Button::PressedCallback callback,
    int setting_accessible_name_id) {
  return delegate_->CreateSettingsButton(std::move(callback),
                                         setting_accessible_name_id);
}

views::Button* TrayDetailedView::CreateHelpButton(
    views::Button::PressedCallback callback) {
  return delegate_->CreateHelpButton(std::move(callback));
}

void TrayDetailedView::HandleViewClicked(views::View* view) {
  NOTREACHED();
}

std::unique_ptr<TriView> TrayDetailedView::CreateTitleTriView(int string_id) {
  auto tri_view = std::make_unique<TriView>(kUnifiedTopShortcutSpacing);

  ConfigureTitleTriView(tri_view.get(), TriView::Container::START);
  ConfigureTitleTriView(tri_view.get(), TriView::Container::CENTER);
  ConfigureTitleTriView(tri_view.get(), TriView::Container::END);

  title_label_ = TrayPopupUtils::CreateDefaultLabel();
  title_label_->SetText(l10n_util::GetStringUTF16(string_id));
  title_label_->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary));
  TrayPopupUtils::SetLabelFontList(title_label_,
                                   TrayPopupUtils::FontStyle::kTitle);
  tri_view->AddView(TriView::Container::CENTER, title_label_);
  tri_view->SetContainerVisible(TriView::Container::END, false);
  tri_view->SetBorder(
      views::CreateEmptyBorder(kUnifiedDetailedViewTitlePadding));

  return tri_view;
}

std::unique_ptr<views::Separator> TrayDetailedView::CreateTitleSeparator() {
  auto separator = std::make_unique<views::Separator>();
  separator->SetColorId(ui::kColorAshSystemUIMenuSeparator);
  separator->SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(
      kTitleRowProgressBarHeight - views::Separator::kThickness, 0, 0, 0)));
  return separator;
}

void TrayDetailedView::CreateExtraTitleRowButtons() {}

void TrayDetailedView::TransitionToMainView() {
  delegate_->TransitionToMainView(back_button_ && back_button_->HasFocus());
}

void TrayDetailedView::CloseBubble() {
  // widget may be null in tests, in this case we do not need to do anything.
  views::Widget* widget = GetWidget();
  if (!widget) {
    return;
  }
  // Don't close again if we're already closing.
  if (widget->IsClosed()) {
    return;
  }
  delegate_->CloseBubble();
}

void TrayDetailedView::IgnoreSeparator() {
  has_separator_ = false;
}

void TrayDetailedView::Layout() {
  views::View::Layout();
  if (scroller_ && !scroller_->is_bounded()) {
    scroller_->ClipHeightTo(0, scroller_->height());
  }
}

int TrayDetailedView::GetHeightForWidth(int width) const {
  if (bounds().IsEmpty()) {
    return views::View::GetHeightForWidth(width);
  }

  // The height of the bubble that contains this detailed view is set to
  // the preferred height of the default view, and that determines the
  // initial height of |this|. Always request to stay the same height.
  return height();
}

const char* TrayDetailedView::GetClassName() const {
  return "TrayDetailedView";
}

void TrayDetailedView::OnThemeChanged() {
  views::View::OnThemeChanged();

  auto* color_provider = AshColorProvider::Get();
  if (title_label_) {
    title_label_->SetEnabledColor(color_provider->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kTextColorPrimary));
  }
  if (sub_header_label_) {
    sub_header_label_->SetEnabledColor(color_provider->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kTextColorPrimary));
  }
  if (sub_header_image_view_) {
    sub_header_image_view_->SetImage(gfx::CreateVectorIcon(
        *sub_header_icon_,
        color_provider->GetContentLayerColor(
            AshColorProvider::ContentLayerType::kIconColorPrimary)));
  }
  if (title_separator_) {
    title_separator_->SetColorId(ui::kColorAshSystemUIMenuSeparator);
  }
}

}  // namespace ash
