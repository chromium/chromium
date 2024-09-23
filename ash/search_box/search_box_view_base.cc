// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/search_box/search_box_view_base.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "ash/assistant/ui/main_stage/launcher_search_iph_view.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/ash_typography.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/ime/text_input_flags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/border.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// The duration for the animation which changes the search icon.
constexpr base::TimeDelta kSearchIconAnimationDuration =
    base::Milliseconds(150);

constexpr int kInnerPadding = 16;

// Padding to make autocomplete ghost text line up with search box text.
constexpr gfx::Insets kGhostTextLabelPadding = gfx::Insets::TLBR(0, 0, 1, 0);

// Preferred width of search box.
constexpr int kSearchBoxPreferredWidth = 544;

// The search box and autocomplete ghost text should be resized but all extra
// space should be allocated to the ghost text category weight views::Label.
constexpr int kSearchBoxWeight = 1;
constexpr int kAutocompleteGhostTextContainerWeight = kSearchBoxPreferredWidth;
constexpr int kAutocompleteGhostTextWeight = 1;
constexpr int kAutocompleteGhostTextCategoryWeight = kSearchBoxPreferredWidth;

// The space between the sunfish button and the assistant button at the edge of
// the search field.
constexpr int kEdgeButtonSpacing = 5;

constexpr SkColor kSearchTextColor = SkColorSetRGB(0x33, 0x33, 0x33);

// The duration for the button fade out animation.
constexpr base::TimeDelta kButtonFadeOutDuration = base::Milliseconds(50);

// The delay for the button fade in animation.
constexpr base::TimeDelta kButtonFadeInDelay = base::Milliseconds(50);

// The duration for the button fade in animation.
constexpr base::TimeDelta kButtonFadeInDuration = base::Milliseconds(100);

void SetupLabelView(views::Label* label,
                    const gfx::FontList& font_list,
                    gfx::Insets border_insets,
                    ui::ColorId color_id) {
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->GetViewAccessibility().SetIsIgnored(true);
  label->SetBackgroundColor(SK_ColorTRANSPARENT);
  label->SetAutoColorReadabilityEnabled(false);
  label->SetEnabledColorId(color_id);
  label->SetFontList(font_list);
  label->SetVisible(true);
  label->SetElideBehavior(gfx::ELIDE_TAIL);
  label->SetMultiLine(false);
  label->SetBorder(views::CreateEmptyBorder(border_insets));
}

}  // namespace

// A background that paints a solid white rounded rect with a thin grey
// border.
class SearchBoxBackground : public views::Background {
 public:
  explicit SearchBoxBackground(int corner_radius)
      : corner_radius_(corner_radius) {}

  SearchBoxBackground(const SearchBoxBackground&) = delete;
  SearchBoxBackground& operator=(const SearchBoxBackground&) = delete;

  ~SearchBoxBackground() override = default;

  void SetCornerRadius(int corner_radius) { corner_radius_ = corner_radius; }

 private:
  // views::Background overrides:
  void Paint(gfx::Canvas* canvas, views::View* view) const override {
    gfx::Rect bounds = view->GetContentsBounds();

    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(get_color());
    canvas->DrawRoundRect(bounds, corner_radius_, flags);
  }

  int corner_radius_;
};

// To paint grey background on mic and back buttons, and close buttons for
// fullscreen launcher.
class SearchBoxImageButton : public views::ImageButton {
  METADATA_HEADER(SearchBoxImageButton, views::ImageButton)

 public:
  explicit SearchBoxImageButton(PressedCallback callback)
      : ImageButton(std::move(callback)) {
    SetFocusBehavior(FocusBehavior::ALWAYS);

    views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);
    SetHasInkDropActionOnClick(true);
    views::InkDrop::UseInkDropForFloodFillRipple(views::InkDrop::Get(this),
                                                 /*highlight_on_hover=*/true);

    SetPreferredSize(gfx::Size(kBubbleLauncherSearchBoxButtonSizeDip,
                               kBubbleLauncherSearchBoxButtonSizeDip));
    SetImageHorizontalAlignment(ALIGN_CENTER);
    SetImageVerticalAlignment(ALIGN_MIDDLE);

    views::InstallCircleHighlightPathGenerator(this);
  }

  SearchBoxImageButton(const SearchBoxImageButton&) = delete;
  SearchBoxImageButton& operator=(const SearchBoxImageButton&) = delete;

  ~SearchBoxImageButton() override {}

  // views::View overrides:
  bool OnKeyPressed(const ui::KeyEvent& event) override {
    // Disable space key to press the button. The keyboard events received
    // by this view are forwarded from a Textfield (SearchBoxView) and key
    // released events are not forwarded. This leaves the button in pressed
    // state.
    if (event.key_code() == ui::VKEY_SPACE)
      return false;

    return Button::OnKeyPressed(event);
  }

  void OnFocus() override {
    views::ImageButton::OnFocus();
    SchedulePaint();
  }

  void OnBlur() override {
    views::ImageButton::OnBlur();
    SchedulePaint();
  }

  void UpdateInkDropColorAndOpacity(SkColor background_color) {
    const std::pair<SkColor, float> base_color_and_opacity =
        ash::ColorProvider::Get()->GetInkDropBaseColorAndOpacity(
            background_color);
    auto* ink_drop = views::InkDrop::Get(this);
    ink_drop->SetBaseColor(base_color_and_opacity.first);
    ink_drop->SetVisibleOpacity(base_color_and_opacity.second);
  }

 private:
  int GetButtonRadius() const { return width() / 2; }
};

BEGIN_METADATA(SearchBoxImageButton)
END_METADATA

// To show context menu of selected view instead of that of focused view which
// is always this view when the user uses keyboard shortcut to open context
// menu.
class SearchBoxTextfield : public views::Textfield {
  // TODO (kylixrd): Add metadata here once the Tast tests are updated.
  // METADATA_HEADER(SearchBoxTextfield, views::Textfield)

 public:
  explicit SearchBoxTextfield(SearchBoxViewBase* search_box_view)
      : search_box_view_(search_box_view) {}

  SearchBoxTextfield(const SearchBoxTextfield&) = delete;
  SearchBoxTextfield& operator=(const SearchBoxTextfield&) = delete;

  ~SearchBoxTextfield() override = default;

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    // Overridden so the BoxLayoutView 'text_container_' can properly layout
    // the search box and ghost text.
    const std::u16string& text = GetText();
    int width = 0;
    int height = 0;
    gfx::Canvas::SizeStringInt(text, GetFontList(), &width, &height, 0,
                               gfx::Canvas::NO_ELLIPSIS);
    gfx::Size size{width + GetCaretBounds().width(), height};
    const auto insets = GetInsets();
    size.Enlarge(insets.width(), insets.height());
    size.SetToMax(gfx::Size(0, 0));
    return size;
  }

  void OnFocus() override {
    search_box_view_->OnSearchBoxFocusedChanged();
    Textfield::OnFocus();
  }

  void OnBlur() override {
    search_box_view_->OnSearchBoxFocusedChanged();
    // Clear selection and set the caret to the end of the text.
    ClearSelection();
    Textfield::OnBlur();

    // Search box focus announcement overlaps with opening or closing folder
    // alert, so we ignored the search box in those cases. Now reset the flag
    // here.
    auto& accessibility = GetViewAccessibility();
    if (accessibility.GetIsIgnored()) {
      accessibility.SetIsIgnored(false);
      NotifyAccessibilityEvent(ax::mojom::Event::kTreeChanged, true);
    }
  }

  void OnGestureEvent(ui::GestureEvent* event) override {
    switch (event->type()) {
      case ui::EventType::kGestureLongPress:
      case ui::EventType::kGestureLongTap:
        // Prevent Long Press from being handled at all, if inactive
        if (!search_box_view_->is_search_box_active()) {
          event->SetHandled();
          break;
        }
        // If |search_box_view_| is active, handle it as normal below
        [[fallthrough]];
      default:
        // Handle all other events as normal
        Textfield::OnGestureEvent(event);
    }
  }

 private:
  const raw_ptr<SearchBoxViewBase> search_box_view_;
};

// TODO (kylixrd): Enable the following once tast-tests are fixed. Metadata
// on this class causes failures since the return value of GetClassName() no
// longer lies and returns the correct class name.
// BEGIN_METADATA(SearchBoxTextfield)
// END_METADATA

// Used to animate the transition between icon images. When a new icon is set,
// this view will temporarily store the layer of the previous icon and animate
// its opacity to fade out, while keeping the correct bounds for the fading out
// layer. At the same time the new icon will fade in.
class SearchIconImageView : public views::ImageView {
  METADATA_HEADER(SearchIconImageView, views::ImageView)

 public:
  SearchIconImageView() = default;

  SearchIconImageView(const SearchIconImageView&) = delete;
  SearchIconImageView& operator=(const SearchIconImageView&) = delete;

  ~SearchIconImageView() override = default;

  // views::View:
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override {
    if (old_icon_layer_)
      old_icon_layer_->SetBounds(layer()->bounds());

    views::ImageView::OnBoundsChanged(previous_bounds);
  }

  void SetSearchIconImage(gfx::ImageSkia image) {
    if (GetImage().isNull() || !animation_enabled_) {
      SetImage(image);
      return;
    }

    if (old_icon_layer_ && old_icon_layer_->GetAnimator()->is_animating())
      old_icon_layer_->GetAnimator()->StopAnimating();

    old_icon_layer_ = RecreateLayer();
    SetImage(image);

    // Animate the old layer to fade out.
    views::AnimationBuilder()
        .SetPreemptionStrategy(
            ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
        .OnEnded(base::BindOnce(&SearchIconImageView::ResetOldIconLayer,
                                weak_factory_.GetWeakPtr()))
        .OnAborted(base::BindOnce(&SearchIconImageView::ResetOldIconLayer,
                                  weak_factory_.GetWeakPtr()))
        .Once()
        .SetDuration(kSearchIconAnimationDuration)
        .SetOpacity(old_icon_layer_.get(), 0.0f, gfx::Tween::EASE_OUT_3);

    // Animate the newly set icon image to fade in.
    layer()->SetOpacity(0.0f);
    views::AnimationBuilder()
        .SetPreemptionStrategy(
            ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
        .Once()
        .SetDuration(kSearchIconAnimationDuration)
        .SetOpacity(layer(), 1.0f, gfx::Tween::EASE_OUT_3);
  }

  void ResetOldIconLayer() { old_icon_layer_.reset(); }

  void set_animation_enabled(bool enabled) { animation_enabled_ = enabled; }

 private:
  std::unique_ptr<ui::Layer> old_icon_layer_;

  bool animation_enabled_ = false;

  base::WeakPtrFactory<SearchIconImageView> weak_factory_{this};
};

BEGIN_METADATA(SearchIconImageView)
END_METADATA

SearchBoxViewBase::InitParams::InitParams() = default;

SearchBoxViewBase::InitParams::~InitParams() = default;

SearchBoxViewBase::SearchBoxViewBase()
    : search_box_(new SearchBoxTextfield(this)) {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  const int between_child_spacing =
      kInnerPadding - views::LayoutProvider::Get()->GetDistanceMetric(
                          views::DISTANCE_TEXTFIELD_HORIZONTAL_TEXT_PADDING);
  main_container_ = AddChildView(std::make_unique<views::BoxLayoutView>());
  main_container_->SetOrientation(views::BoxLayout::Orientation::kVertical);
  main_container_->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);
  main_container_->SetMainAxisAlignment(
      views::BoxLayout::MainAxisAlignment::kCenter);

  content_container_ =
      main_container_->AddChildView(std::make_unique<views::BoxLayoutView>());
  content_container_->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  content_container_->SetMinimumCrossAxisSize(kSearchBoxPreferredHeight);
  content_container_->SetOrientation(
      views::BoxLayout::Orientation::kHorizontal);
  content_container_->SetInsideBorderInsets(
      gfx::Insets::VH(0, kSearchBoxPadding));
  content_container_->SetBetweenChildSpacing(between_child_spacing);

  search_icon_ =
      content_container_->AddChildView(std::make_unique<SearchIconImageView>());
  search_icon_->SetPaintToLayer();
  search_icon_->layer()->SetFillsBoundsOpaquely(false);

  search_box_->SetBorder(views::NullBorder());
  search_box_->SetBackgroundColor(SK_ColorTRANSPARENT);
  search_box_->set_controller(this);
  search_box_->SetTextInputType(ui::TEXT_INPUT_TYPE_SEARCH);
  search_box_->SetTextInputFlags(ui::TEXT_INPUT_FLAG_AUTOCORRECT_OFF);
  auto font_list = search_box_->GetFontList().DeriveWithSizeDelta(2);
  SetPreferredStyleForSearchboxText(font_list, kSearchTextColor);
  search_box_->SetCursorEnabled(is_search_box_active_);

  text_container_ = content_container_->AddChildView(
      std::make_unique<views::BoxLayoutView>());
  text_container_->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  text_container_->SetMinimumCrossAxisSize(kSearchBoxPreferredHeight);
  text_container_->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  content_container_->SetFlexForView(text_container_, 1,
                                     /*use_min_size=*/false);

  text_container_->AddChildView(search_box_.get());
  ghost_text_container_ =
      text_container_->AddChildView(std::make_unique<views::BoxLayoutView>());
  ghost_text_container_->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  ghost_text_container_->SetMinimumCrossAxisSize(kSearchBoxPreferredHeight);
  ghost_text_container_->SetOrientation(
      views::BoxLayout::Orientation::kHorizontal);
  ghost_text_container_->SetVisible(false);

  text_container_->SetFlexForView(search_box_, kSearchBoxWeight,
                                  /*use_min_size=*/true);
  text_container_->SetFlexForView(ghost_text_container_,
                                  kAutocompleteGhostTextContainerWeight,
                                  /*use_min_size=*/true);

  separator_label_ =
      ghost_text_container_->AddChildView(std::make_unique<views::Label>());
  autocomplete_ghost_text_ =
      ghost_text_container_->AddChildView(std::make_unique<views::Label>());
  category_separator_label_ =
      ghost_text_container_->AddChildView(std::make_unique<views::Label>());
  category_ghost_text_ =
      ghost_text_container_->AddChildView(std::make_unique<views::Label>());

  SetPreferredStyleForAutocompleteText(font_list, kColorAshTextColorSuggestion);

  separator_label_->SetText(
      l10n_util::GetStringUTF16(IDS_ASH_SEARCH_RESULT_SEPARATOR));
  category_separator_label_->SetText(
      l10n_util::GetStringUTF16(IDS_ASH_SEARCH_RESULT_SEPARATOR));

  ghost_text_container_->SetFlexForView(autocomplete_ghost_text_,
                                        kAutocompleteGhostTextWeight,
                                        /*use_min_size=*/true);
  ghost_text_container_->SetFlexForView(category_ghost_text_,
                                        kAutocompleteGhostTextCategoryWeight,
                                        /*use_min_size=*/true);

  // |search_box_button_container_| which will show either the assistant button,
  // the close button, or nothing on the right side of the search box view.
  search_box_button_container_ =
      content_container_->AddChildView(std::make_unique<views::View>());
  search_box_button_container_->SetLayoutManager(
      std::make_unique<views::FillLayout>());
  content_container_->SetFlexForView(search_box_button_container_, 0,
                                     /*use_min_size=*/true);

  UpdateSearchTextfieldAccessibleActiveDescendantId();
}

SearchBoxViewBase::~SearchBoxViewBase() = default;

void SearchBoxViewBase::Init(const InitParams& params) {
  show_close_button_when_active_ = params.show_close_button_when_active;
  search_icon_->set_animation_enabled(params.animate_changing_search_icon);
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetMasksToBounds(true);
  if (params.create_background) {
    SetBackground(
        std::make_unique<SearchBoxBackground>(kSearchBoxBorderCornerRadius));
  }

  if (params.increase_child_view_padding) {
    content_container_->SetBetweenChildSpacing(kInnerPadding);
  }

  if (params.textfield_margins) {
    search_box()->SetProperty(views::kMarginsKey, *params.textfield_margins);
  }

  UpdateSearchBoxBorder();
}

views::ImageButton* SearchBoxViewBase::CreateCloseButton(
    const base::RepeatingClosure& button_callback) {
  MaybeCreateFilterAndCloseButtonContainer();

  DCHECK(!close_button_);
  close_button_ = filter_and_close_button_container_->AddChildView(
      std::make_unique<SearchBoxImageButton>(button_callback));
  return close_button_;
}

void SearchBoxViewBase::CreateEndButtonContainer() {
  CHECK(!end_button_container_);

  // `end_button_container_` is used to align the assistant button to the
  // end of the `search_box_button_container_`.
  end_button_container_ = search_box_button_container_->AddChildView(
      std::make_unique<views::BoxLayoutView>());
  end_button_container_->SetMainAxisAlignment(
      views::BoxLayout::MainAxisAlignment::kEnd);
  end_button_container_->SetBetweenChildSpacing(kEdgeButtonSpacing);
  end_button_container_->SetPaintToLayer();
  end_button_container_->layer()->SetFillsBoundsOpaquely(false);
  end_button_container_->SetVisible(false);
}

views::ImageButton* SearchBoxViewBase::CreateSunfishButton(
    const base::RepeatingClosure& button_callback) {
  CHECK(end_button_container_);
  CHECK(!sunfish_button_);

  sunfish_button_ = end_button_container_->AddChildView(
      std::make_unique<SearchBoxImageButton>(button_callback));
  return sunfish_button_;
}

views::ImageButton* SearchBoxViewBase::CreateAssistantButton(
    const base::RepeatingClosure& button_callback) {
  CHECK(end_button_container_);
  CHECK(!assistant_button_);

  assistant_button_ = end_button_container_->AddChildView(
      std::make_unique<SearchBoxImageButton>(button_callback));
  return assistant_button_;
}

views::ImageButton* SearchBoxViewBase::CreateFilterButton(
    const base::RepeatingClosure& button_callback) {
  MaybeCreateFilterAndCloseButtonContainer();

  DCHECK(!filter_button_);
  filter_button_ = filter_and_close_button_container_->AddChildView(
      std::make_unique<SearchBoxImageButton>(button_callback));
  filter_button_->GetViewAccessibility().SetRole(ax::mojom::Role::kPopUpButton);
  filter_button_->GetViewAccessibility().SetHasPopup(
      ax::mojom::HasPopup::kMenu);
  return filter_button_;
}

bool SearchBoxViewBase::HasSearch() const {
  return !search_box_->GetText().empty();
}

gfx::Rect SearchBoxViewBase::GetViewBoundsForSearchBoxContentsBounds(
    const gfx::Rect& rect) const {
  gfx::Rect view_bounds = rect;
  view_bounds.Inset(-GetInsets());
  return view_bounds;
}

views::ImageButton* SearchBoxViewBase::assistant_button() {
  return assistant_button_;
}

views::ImageButton* SearchBoxViewBase::sunfish_button() {
  return sunfish_button_;
}

views::View* SearchBoxViewBase::edge_button_container() {
  return end_button_container_;
}

views::ImageButton* SearchBoxViewBase::close_button() {
  return close_button_;
}

views::ImageButton* SearchBoxViewBase::filter_button() {
  return filter_button_;
}

views::View* SearchBoxViewBase::filter_and_close_button_container() {
  return filter_and_close_button_container_;
}

views::ImageView* SearchBoxViewBase::search_icon() {
  return search_icon_;
}

void SearchBoxViewBase::SetIphView(
    std::unique_ptr<LauncherSearchIphView> view) {
  if (GetIphView()) {
    DCHECK(false) << "SetIphView gets called with an IPH view being shown.";

    DeleteIphView();
  }

  iph_view_tracker_.SetView(main_container_->AddChildView(std::move(view)));
}

LauncherSearchIphView* SearchBoxViewBase::GetIphView() {
  views::View* view = iph_view_tracker_.view();
  if (!view) {
    return nullptr;
  }

  CHECK(views::IsViewClass<LauncherSearchIphView>(view))
      << "Only LaunchserSearchIph view is supported now";
  return static_cast<LauncherSearchIphView*>(view);
}

void SearchBoxViewBase::DeleteIphView() {
  main_container_->RemoveChildViewT(GetIphView());
}

void SearchBoxViewBase::TriggerSearch() {
  HandleQueryChange(search_box_->GetText(), /*initiated_by_user=*/false);
}

void SearchBoxViewBase::MaybeSetAutocompleteGhostText(
    const std::u16string& title,
    const std::u16string& category) {
  if (title.empty() && category.empty()) {
    ghost_text_container_->SetVisible(false);
    autocomplete_ghost_text_->SetText(std::u16string());
    category_ghost_text_->SetText(std::u16string());
  } else {
    ghost_text_container_->SetVisible(true);
    autocomplete_ghost_text_->SetText(title);
    separator_label_->SetVisible(!title.empty());
    category_ghost_text_->SetText(category);
    category_separator_label_->SetVisible(!category.empty());
  }
}

std::string SearchBoxViewBase::GetSearchBoxGhostTextForTest() {
  if (!autocomplete_ghost_text_->GetText().empty()) {
    return base::UTF16ToUTF8(base::StrCat(
        {autocomplete_ghost_text_->GetText(),
         l10n_util::GetStringUTF16(IDS_ASH_SEARCH_RESULT_SEPARATOR),
         category_ghost_text_->GetText()}));
  }
  return base::UTF16ToUTF8(category_ghost_text_->GetText());
}

void SearchBoxViewBase::SetSearchBoxActive(bool active,
                                           ui::EventType event_type) {
  if (active == is_search_box_active_)
    return;

  is_search_box_active_ = active;
  UpdatePlaceholderTextStyle();
  search_box_->SetCursorEnabled(active);

  if (active) {
    search_box_->RequestFocus();
  } else {
    search_box_->DestroyTouchSelection();
  }

  UpdateSearchBoxBorder();
  // Keep the current keyboard visibility if the user already started typing.
  if (event_type != ui::EventType::kKeyPressed &&
      event_type != ui::EventType::kKeyReleased) {
    UpdateKeyboardVisibility();
  }
  UpdateButtonsVisibility();
  OnSearchBoxActiveChanged(active);

  content_container_->DeprecatedLayoutImmediately();
  UpdateSearchBoxFocusPaint();
  SchedulePaint();
}

bool SearchBoxViewBase::OnTextfieldEvent(ui::EventType type) {
  if (is_search_box_active_)
    return false;

  SetSearchBoxActive(true, type);
  return true;
}

gfx::Size SearchBoxViewBase::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  views::SizeBounds content_available_size(available_size);
  gfx::Insets insets = GetInsets();
  content_available_size.Enlarge(-insets.width(), -insets.height());
  const int iph_height = iph_view_tracker_.view()
                             ? iph_view_tracker_.view()
                                   ->GetPreferredSize(content_available_size)
                                   .height()
                             : 0;
  return gfx::Size(kSearchBoxPreferredWidth,
                   kSearchBoxPreferredHeight + insets.height() + iph_height);
}

void SearchBoxViewBase::OnEnabledChanged() {
  bool enabled = GetEnabled();
  search_box_->SetEnabled(enabled);
  if (close_button_)
    close_button_->SetEnabled(enabled);
  if (assistant_button_)
    assistant_button_->SetEnabled(enabled);
  if (filter_button_) {
    filter_button_->SetEnabled(enabled);
  }
}

void SearchBoxViewBase::OnGestureEvent(ui::GestureEvent* event) {
  HandleSearchBoxEvent(event);
}

void SearchBoxViewBase::OnMouseEvent(ui::MouseEvent* event) {
  HandleSearchBoxEvent(event);
}

void SearchBoxViewBase::OnThemeChanged() {
  views::View::OnThemeChanged();
  search_box_->SetSelectionBackgroundColor(
      GetWidget()->GetColorProvider()->GetColor(kColorAshFocusAuraColor));
  UpdatePlaceholderTextStyle();
}

void SearchBoxViewBase::NotifyGestureEvent() {
  search_box_->DestroyTouchSelection();
}

void SearchBoxViewBase::OnSearchBoxFocusedChanged() {
  UpdateSearchBoxBorder();
  DeprecatedLayoutImmediately();
  UpdateSearchBoxFocusPaint();
  SchedulePaint();
}

bool SearchBoxViewBase::IsSearchBoxTrimmedQueryEmpty() const {
  std::u16string trimmed_query;
  base::TrimWhitespace(search_box_->GetText(), base::TrimPositions::TRIM_ALL,
                       &trimmed_query);
  return trimmed_query.empty();
}

void SearchBoxViewBase::UpdateSearchTextfieldAccessibleActiveDescendantId() {}

void SearchBoxViewBase::ClearSearch() {
  search_box_->SetText(std::u16string());
  UpdateButtonsVisibility();
  HandleQueryChange(u"", /*initiated_by_user=*/false);
}

void SearchBoxViewBase::OnSearchBoxActiveChanged(bool active) {}

void SearchBoxViewBase::UpdateSearchBoxFocusPaint() {}

void SearchBoxViewBase::SetPreferredStyleForAutocompleteText(
    const gfx::FontList& font_list,
    ui::ColorId text_color_id) {
  SetupLabelView(separator_label_, font_list, kGhostTextLabelPadding,
                 text_color_id);
  SetupLabelView(autocomplete_ghost_text_, font_list, kGhostTextLabelPadding,
                 text_color_id);
  SetupLabelView(category_separator_label_, font_list, kGhostTextLabelPadding,
                 text_color_id);
  SetupLabelView(category_ghost_text_, font_list, kGhostTextLabelPadding,
                 text_color_id);
}

void SearchBoxViewBase::SetPreferredStyleForSearchboxText(
    const gfx::FontList& font_list,
    ui::ColorId text_color_id) {
  search_box_->SetTextColor(text_color_id);
  search_box_->SetFontList(font_list);
}

void SearchBoxViewBase::UpdateButtonsVisibility() {
  DCHECK(close_button_);

  const bool should_show_close_button =
      !search_box_->GetText().empty() ||
      (show_close_button_when_active_ && is_search_box_active_);

  if (should_show_close_button) {
    MaybeFadeContainerIn(filter_and_close_button_container_);
  } else {
    MaybeFadeContainerOut(filter_and_close_button_container_);
  }

  if (assistant_button_ || sunfish_button_) {
    const bool should_show_edge_buttons =
        (show_assistant_button_ || show_sunfish_button_) &&
        !should_show_close_button;
    if (should_show_edge_buttons) {
      MaybeFadeContainerIn(end_button_container_);
    } else {
      MaybeFadeContainerOut(end_button_container_);
    }
  }

  // Explicitly call Layout as the number of the buttons showing may change.
  InvalidateLayout();
}

void SearchBoxViewBase::MaybeFadeContainerIn(views::View* container) {
  CHECK(container);
  if (container->GetVisible() &&
      container->layer()->GetTargetOpacity() == 1.0f) {
    return;
  }

  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .Once()
      .At(kButtonFadeInDelay)
      .SetDuration(kButtonFadeInDuration)
      .SetOpacity(container->layer(), 1.0f, gfx::Tween::LINEAR);

  // Set the container visible after scheduling the animation because scheduling
  // the animation might abort a fade-out, which sets the container invisible.
  container->SetVisible(true);
}

void SearchBoxViewBase::MaybeFadeContainerOut(views::View* container) {
  CHECK(container);
  if (!container->GetVisible() || container->layer()->GetTargetOpacity() == 0) {
    return;
  }

  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .OnEnded(base::BindOnce(&SearchBoxViewBase::SetContainerVisibilityHidden,
                              weak_factory_.GetWeakPtr(), container))
      .OnAborted(
          base::BindOnce(&SearchBoxViewBase::SetContainerVisibilityHidden,
                         weak_factory_.GetWeakPtr(), container))
      .Once()
      .SetDuration(kButtonFadeOutDuration)
      .SetOpacity(container->layer(), 0.0f, gfx::Tween::LINEAR);
}

void SearchBoxViewBase::SetContainerVisibilityHidden(views::View* container) {
  container->SetVisible(false);
}

void SearchBoxViewBase::ContentsChanged(views::Textfield* sender,
                                        const std::u16string& new_contents) {
  // Set search box focused when query changes.
  search_box_->RequestFocus();
  HandleQueryChange(new_contents, /*initiated_by_user=*/true);
  if (!new_contents.empty())
    SetSearchBoxActive(true, ui::EventType::kKeyPressed);
  UpdateButtonsVisibility();
}

bool SearchBoxViewBase::HandleMouseEvent(views::Textfield* sender,
                                         const ui::MouseEvent& mouse_event) {
  return OnTextfieldEvent(mouse_event.type());
}

bool SearchBoxViewBase::HandleGestureEvent(
    views::Textfield* sender,
    const ui::GestureEvent& gesture_event) {
  return OnTextfieldEvent(gesture_event.type());
}

void SearchBoxViewBase::SetSearchBoxBackgroundCornerRadius(int corner_radius) {
  auto* search_box_background = static_cast<SearchBoxBackground*>(background());
  if (search_box_background)
    search_box_background->SetCornerRadius(corner_radius);
}

void SearchBoxViewBase::SetSearchIconImage(gfx::ImageSkia image) {
  search_icon_->SetSearchIconImage(image);
}

void SearchBoxViewBase::SetShowAssistantButton(bool show) {
  DCHECK(assistant_button_);
  show_assistant_button_ = show;
  assistant_button_->SetVisible(show);
  UpdateButtonsVisibility();
}

void SearchBoxViewBase::SetShowSunfishButton(bool show) {
  DCHECK(features::IsSunfishFeatureEnabled() && sunfish_button_);
  show_sunfish_button_ = show;
  sunfish_button_->SetVisible(show);
  UpdateButtonsVisibility();
}

void SearchBoxViewBase::HandleSearchBoxEvent(ui::LocatedEvent* located_event) {
  if (located_event->type() == ui::EventType::kMousePressed ||
      located_event->type() == ui::EventType::kGestureTap) {
    const bool event_is_in_searchbox_bounds =
        GetBoundsInScreen().Contains(located_event->root_location());
    if (!event_is_in_searchbox_bounds)
      return;

    located_event->SetHandled();

    // If the event is in an inactive empty search box, enable the search box.
    if (!is_search_box_active_ && search_box_->GetText().empty()) {
      SetSearchBoxActive(true, located_event->type());
      return;
    }

    // Otherwise, update the keyboard in case it was hidden. Tapping again
    // should reopen it.
    UpdateKeyboardVisibility();
  }
}

void SearchBoxViewBase::UpdateBackgroundColor(SkColor color) {
  auto* search_box_background = background();
  if (search_box_background)
    search_box_background->SetNativeControlColor(color);
  if (close_button_)
    close_button_->UpdateInkDropColorAndOpacity(color);
  if (assistant_button_)
    assistant_button_->UpdateInkDropColorAndOpacity(color);
  if (filter_button_) {
    filter_button_->UpdateInkDropColorAndOpacity(color);
  }
}

void SearchBoxViewBase::MaybeCreateFilterAndCloseButtonContainer() {
  if (!filter_and_close_button_container_) {
    filter_and_close_button_container_ =
        search_box_button_container_->AddChildView(
            std::make_unique<views::BoxLayoutView>());
    filter_and_close_button_container_->SetPaintToLayer();
    filter_and_close_button_container_->layer()->SetFillsBoundsOpaquely(false);
    filter_and_close_button_container_->SetVisible(false);
  }
}

BEGIN_METADATA(SearchBoxViewBase)
END_METADATA

}  // namespace ash
