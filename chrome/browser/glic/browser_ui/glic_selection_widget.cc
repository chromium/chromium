// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/browser_ui/glic_selection_widget.h"

#include "base/strings/strcat.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/resources/grit/glic_browser_resources.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/tabs/public/tab_interface.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/color/color_variant.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/screen.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/text_elider.h"
#include "ui/gfx/text_utils.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/button/menu_button_controller.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/menu/menu_scroll_view_container.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"
#include "ui/views/view_tracker.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace glic {

namespace {

constexpr size_t kMaxSelectionLengthForTooltip = 50;
constexpr int kIconSize = 14;

constexpr int kCornerRadius = 12;
// The two pills are visually grouped together by having a smaller border radius
// on the sides where they meet.
constexpr int kCornerRadiusInner = 4;
constexpr int kMenuVerticalMargin = 4;

// Custom MenuModel for the selection widget's three-dot menu, providing custom
// typography styling for the menu items.
class GlicSelectionMenuModel : public ui::SimpleMenuModel {
 public:
  using ui::SimpleMenuModel::SimpleMenuModel;

  const gfx::FontList* GetLabelFontListAt(size_t index) const override {
    return &views::TypographyProvider::Get().GetFont(
        views::style::CONTEXT_BUTTON, views::style::STYLE_BODY_5_MEDIUM);
  }
};

// Adapter that binds the `GlicSelectionMenuModel` to menu views. It intercepts
// the menu showing process to customize the border of the menu container and
// adjust vertical padding on menu items.
class GlicSelectionMenuModelAdapter : public views::MenuModelAdapter {
 public:
  GlicSelectionMenuModelAdapter(ui::MenuModel* menu_model,
                                views::View* anchor_view,
                                int visual_top_of_chip,
                                const gfx::Rect& menu_button_bounds)
      : views::MenuModelAdapter(menu_model),
        anchor_view_(anchor_view),
        visual_top_of_chip_(visual_top_of_chip),
        menu_button_bounds_(menu_button_bounds) {}

  void WillShowMenu(views::MenuItemView* menu) override {
    views::MenuModelAdapter::WillShowMenu(menu);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&GlicSelectionMenuModelAdapter::CustomizeMenuBorder,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::make_unique<views::ViewTracker>(menu)));
  }

  std::optional<SkColor> GetLabelColor(int command_id) const override {
    if (anchor_view_ && anchor_view_->GetWidget()) {
      if (auto* cp = anchor_view_->GetColorProvider()) {
        return cp->GetColor(ui::kColorSysOnSurfaceVariant);
      }
    }
    return views::MenuModelAdapter::GetLabelColor(command_id);
  }

 protected:
  views::MenuItemView* AppendMenuItem(views::MenuItemView* menu,
                                      ui::MenuModel* model,
                                      size_t index) override {
    views::MenuItemView* item =
        views::MenuModelAdapter::AppendMenuItem(menu, model, index);
    if (item) {
      item->set_vertical_margin(kMenuVerticalMargin);
    }
    return item;
  }

 private:
  void CustomizeMenuBorder(std::unique_ptr<views::ViewTracker> tracker) {
    views::View* tracked_view = tracker->view();
    if (!tracked_view) {
      return;
    }
    views::MenuItemView* menu = static_cast<views::MenuItemView*>(tracked_view);
    if (!menu || !menu->GetSubmenu()) {
      return;
    }
    views::MenuScrollViewContainer* container =
        menu->GetSubmenu()->GetScrollViewContainer();
    if (!container || !container->GetWidget()) {
      return;
    }
    // Calculate additional vertical insets added by MenuScrollViewContainer's
    // rounded corners implementation so we can subtract them from the bubble
    // border insets to make the menu more compact.
    const views::MenuConfig& menu_config = views::MenuConfig::instance();
    int border_radius =
        menu_config.CornerRadiusForMenu(menu->GetMenuController());
    int border_thickness = menu_config.use_outer_border ? 1 : 0;
    gfx::Insets additional_insets =
        gfx::Insets::VH(menu_config.rounded_menu_vertical_border_size.value_or(
                            border_radius),
                        menu_config.menu_horizontal_border_size) -
        gfx::Insets(border_thickness);

    constexpr int kVerticalPadding = 8;
    int subtract_vertical =
        std::max(0, additional_insets.top() - kVerticalPadding);
    gfx::Insets custom_subtraction =
        gfx::Insets::TLBR(subtract_vertical, 0, subtract_vertical, 0);

    auto bubble_border = std::make_unique<views::BubbleBorder>(
        views::BubbleBorder::NONE, views::BubbleBorder::STANDARD_SHADOW);
    bubble_border->SetColor(ui::kColorSysSurface);
    bubble_border->set_rounded_corners(gfx::RoundedCornersF(kCornerRadius));

    gfx::Insets insets = bubble_border->GetInsets() - custom_subtraction;
    bubble_border->set_insets(gfx::Insets::TLBR(
        std::max(0, insets.top()), std::max(0, insets.left()),
        std::max(0, insets.bottom()), std::max(0, insets.right())));

    auto* raw_border = bubble_border.get();
    container->SetBorder(std::move(bubble_border));
    container->SetBackground(
        std::make_unique<views::BubbleBackground>(raw_border));

    views::Widget* menu_widget = container->GetWidget();
    if (menu_widget) {
      int widget_y = visual_top_of_chip_ - raw_border->GetInsets().top();
      // Align the visual left of the menu with the visual left of the menu
      // button.
      int widget_x = menu_button_bounds_.x() - raw_border->GetInsets().left();
      gfx::Size pref_size = container->GetPreferredSize();
      gfx::Rect menu_rect(widget_x, widget_y, pref_size.width(),
                          pref_size.height());
      gfx::Rect monitor_area =
          display::Screen::Get()
              ->GetDisplayNearestPoint(menu_button_bounds_.origin())
              .work_area();
      menu_rect.AdjustToFit(monitor_area);
      menu_widget->SetBounds(menu_rect);
    }
  }

  const raw_ptr<views::View> anchor_view_;
  const int visual_top_of_chip_;
  const gfx::Rect menu_button_bounds_;
  base::WeakPtrFactory<GlicSelectionMenuModelAdapter> weak_ptr_factory_{this};
};

class GlicSelectionContentsView : public views::View,
                                  public ui::SimpleMenuModel::Delegate {
  METADATA_HEADER(GlicSelectionContentsView, views::View)

 public:
  GlicSelectionContentsView(GlicSelectionWidgetDelegate* widget_delegate,
                            const std::u16string& selected_text,
                            bool initial_pinned_state)
      : widget_delegate_(widget_delegate) {
    SetNotifyEnterExitOnChild(true);

    auto border1 = std::make_unique<views::BubbleBorder>(
        views::BubbleBorder::NONE, views::BubbleBorder::STANDARD_SHADOW);
    border1->SetColor(ui::kColorSysSurface);
    border1->set_rounded_corners(gfx::RoundedCornersF(kCornerRadius));

    // BubbleBorders add a shadow inset on all sides. We use a negative
    // spacing here so the visible backgrounds of the pills are closer together
    // without their shadow insets pushing them far apart.
    constexpr int kVisualSpacing = 2;
    int spacing = kVisualSpacing - border1->GetInsets().right() -
                  border1->GetInsets().left();

    auto layout = std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal, gfx::Insets(0), spacing);
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
    SetLayoutManager(std::move(layout));

    ask_pill_ = AddChildView(std::make_unique<views::BoxLayoutView>());
    ask_pill_->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
    ask_pill_->SetInsideBorderInsets(gfx::Insets::VH(4, 4));
    ask_pill_->SetBetweenChildSpacing(4);
    ask_pill_->SetCrossAxisAlignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
    ask_pill_->SetBackground(
        std::make_unique<views::BubbleBackground>(border1.get()));
    ask_pill_->SetBorder(std::move(border1));

    // Ask Gemini Button
    std::u16string truncated_text;
    if (selected_text.length() <= kMaxSelectionLengthForTooltip) {
      truncated_text = selected_text;
    } else {
      truncated_text = gfx::StringSlicer(selected_text, gfx::kEllipsisUTF16,
                                         /*elide_in_middle=*/true,
                                         /*elide_at_beginning=*/false)
                           .CutString(kMaxSelectionLengthForTooltip,
                                      /*insert_ellipsis=*/true);
    }
    auto ask_gemini_tooltip = l10n_util::GetStringFUTF16(
        IDS_GLIC_SELECTION_ASK_ABOUT,
        base::StrCat({u"\"", truncated_text, u"\""}));
    auto* ask_gemini_btn =
        ask_pill_->AddChildView(std::make_unique<views::MdTextButton>(
            base::BindRepeating(
                &GlicSelectionWidgetDelegate::ActionDelegate::OnAskGemini,
                base::Unretained(&widget_delegate_->action_delegate())),
            l10n_util::GetStringUTF16(
                IDS_GLIC_BUTTON_ENTRYPOINT_ASK_GEMINI_LABEL)));
    ask_gemini_btn->SetStyle(ui::ButtonStyle::kText);
    ask_gemini_btn->SetTooltipText(ask_gemini_tooltip);
    ask_gemini_btn->SetImageLabelSpacing(4);
    ask_gemini_btn->SetEnabledTextColors(ui::kColorSysOnSurfaceVariant);
    ask_gemini_btn->SetTextColor(views::Button::STATE_DISABLED,
                                 ui::kColorLabelForegroundDisabled);
    ask_gemini_btn->SetLabelStyle(views::style::STYLE_BODY_5_MEDIUM);
    ask_gemini_btn->SetCustomPadding(gfx::Insets::TLBR(0, 2, 0, 6));

    gfx::ImageSkia* icon_skia =
        ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
            IDR_GLIC_BUTTON_ALT_ICON);
    gfx::ImageSkia resized_icon = gfx::ImageSkiaOperations::CreateResizedImage(
        *icon_skia, skia::ImageOperations::RESIZE_BEST,
        gfx::Size(kIconSize, kIconSize));

    auto generator = base::BindRepeating(
        [](gfx::ImageSkia icon, const ui::ColorProvider* color_provider) {
          if (!color_provider) {
            return icon;
          }
          SkColor circle_bg_color =
              color_provider->GetColor(ui::kColorSysBaseContainer);
          return gfx::ImageSkiaOperations::CreateImageWithCircleBackground(
              10, circle_bg_color, icon);
        },
        resized_icon);

    auto icon_model = ui::ImageModel::FromImageGenerator(std::move(generator),
                                                         gfx::Size(20, 20));

    ask_gemini_btn->SetImageModel(views::Button::STATE_NORMAL, icon_model);
    ask_gemini_btn->SetImageModel(views::Button::STATE_HOVERED, icon_model);
    ask_gemini_btn->SetImageModel(views::Button::STATE_PRESSED, icon_model);
    ask_gemini_btn->SetImageModel(views::Button::STATE_DISABLED, icon_model);

    views::InkDrop::Get(ask_gemini_btn)
        ->SetMode(views::InkDropHost::InkDropMode::ON);
    ask_gemini_btn->SetHasInkDropActionOnClick(true);
    ask_gemini_btn->SetShowInkDropWhenHotTracked(true);
    views::InstallRoundRectHighlightPathGenerator(ask_gemini_btn, gfx::Insets(),
                                                  kCornerRadius);
    views::InkDrop::Get(ask_gemini_btn)
        ->SetBaseColorCallback(base::BindRepeating(
            [](views::View* host) {
              return host->GetColorProvider()->GetColor(ui::kColorSysOnSurface);
            },
            base::Unretained(ask_gemini_btn)));
    CreateToolbarInkdropCallbacks(ask_gemini_btn, kColorToolbarInkDropHover,
                                  kColorToolbarInkDropRipple);

    // Copy Button
    auto copy_tooltip = gfx::LocateAndRemoveAcceleratorChar(
        l10n_util::GetStringUTF16(IDS_APP_COPY), nullptr, nullptr);
    auto* copy_btn =
        ask_pill_->AddChildView(views::ImageButton::CreateIconButton(
            base::BindRepeating(
                &GlicSelectionWidgetDelegate::ActionDelegate::OnCopy,
                base::Unretained(&widget_delegate_->action_delegate())),
            features::IsRoundedIconsEnabled()
                ? vector_icons::kContentCopyIcon
                : vector_icons::kContentCopyOldIcon,
            copy_tooltip));
    copy_btn->SetTooltipText(copy_tooltip);
    copy_btn->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
    copy_btn->SetBorder(
        views::CreateEmptyBorder(views::LayoutProvider::Get()->GetInsetsMetric(
            views::INSETS_VECTOR_IMAGE_BUTTON)));
    views::SetImageFromVectorIconWithColor(
        copy_btn,
        features::IsRoundedIconsEnabled() ? vector_icons::kContentCopyIcon
                                          : vector_icons::kContentCopyOldIcon,
        kIconSize,
        views::IconColors(ui::kColorSysOnSurfaceVariant,
                          ui::kColorLabelForegroundDisabled,
                          ui::kColorSysOnSurfaceVariant));
    CreateToolbarInkdropCallbacks(copy_btn, kColorToolbarInkDropHover,
                                  kColorToolbarInkDropRipple);

    // Copy Link Button
    auto copy_link_tooltip =
        l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_COPYLINKTOTEXT);
    copy_link_btn_ =
        ask_pill_->AddChildView(views::ImageButton::CreateIconButton(
            base::BindRepeating(
                &GlicSelectionWidgetDelegate::ActionDelegate::OnCopyLink,
                base::Unretained(&widget_delegate_->action_delegate())),
            features::IsRoundedIconsEnabled()
                ? omnibox::kShareIcon
                : omnibox::kShareChromeRefreshOldIcon,
            copy_link_tooltip));
    copy_link_btn_->SetTooltipText(copy_link_tooltip);
    copy_link_btn_->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
    copy_link_btn_->SetBorder(
        views::CreateEmptyBorder(views::LayoutProvider::Get()->GetInsetsMetric(
            views::INSETS_VECTOR_IMAGE_BUTTON)));
    views::SetImageFromVectorIconWithColor(
        copy_link_btn_,
        features::IsRoundedIconsEnabled() ? omnibox::kShareIcon
                                          : omnibox::kShareChromeRefreshOldIcon,
        kIconSize,
        views::IconColors(ui::kColorSysOnSurfaceVariant,
                          ui::kColorLabelForegroundDisabled,
                          ui::kColorSysOnSurfaceVariant));
    CreateToolbarInkdropCallbacks(copy_link_btn_, kColorToolbarInkDropHover,
                                  kColorToolbarInkDropRipple);
    copy_link_btn_->SetEnabled(false);

    // Pill 2
    // TODO(b/520398290): Will change dismiss_pill_ to control_pill_.
    dismiss_pill_ = AddChildView(std::make_unique<views::BoxLayoutView>());
    dismiss_pill_->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
    dismiss_pill_->SetInsideBorderInsets(gfx::Insets::VH(4, 4));
    dismiss_pill_->SetCrossAxisAlignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
    auto border2 = std::make_unique<views::BubbleBorder>(
        views::BubbleBorder::NONE, views::BubbleBorder::STANDARD_SHADOW);
    border2->SetColor(ui::kColorSysSurface);
    border2->set_rounded_corners(gfx::RoundedCornersF(
        kCornerRadiusInner, kCornerRadius, kCornerRadius, kCornerRadiusInner));
    dismiss_pill_->SetBackground(
        std::make_unique<views::BubbleBackground>(border2.get()));
    dismiss_pill_->SetBorder(std::move(border2));

    // Pin/Unpin Toggle Button or Dismiss Button
    if (features::kGlicSelectionPromptEnablePinning.Get()) {
      pin_btn_ =
          dismiss_pill_->AddChildView(views::ImageButton::CreateIconButton(
              base::BindRepeating(&GlicSelectionWidgetDelegate::TogglePinState,
                                  base::Unretained(widget_delegate_)),
              features::IsRoundedIconsEnabled()
                  ? vector_icons::kKeyboardArrowUpIcon
                  : vector_icons::kCaretUpOldIcon,
              std::u16string()));
      pin_btn_->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
      pin_btn_->SetBorder(views::CreateEmptyBorder(
          views::LayoutProvider::Get()->GetInsetsMetric(
              views::INSETS_VECTOR_IMAGE_BUTTON)));
      CreateToolbarInkdropCallbacks(pin_btn_, kColorToolbarInkDropHover,
                                    kColorToolbarInkDropRipple);
      SetPinned(initial_pinned_state);
    } else {
      auto menu_tooltip = l10n_util::GetStringUTF16(IDS_TOAST_MENU_BUTTON_NAME);
      menu_btn_ =
          dismiss_pill_->AddChildView(views::ImageButton::CreateIconButton(
              base::RepeatingClosure(), kMoreVertIcon, menu_tooltip));
      menu_btn_->SetButtonController(
          std::make_unique<views::MenuButtonController>(
              menu_btn_,
              base::BindRepeating(
                  &GlicSelectionContentsView::OnMenuButtonClicked,
                  base::Unretained(this)),
              std::make_unique<views::Button::DefaultButtonControllerDelegate>(
                  menu_btn_)));
      menu_btn_->SetTooltipText(menu_tooltip);
      menu_btn_->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
      menu_btn_->SetBorder(views::CreateEmptyBorder(
          views::LayoutProvider::Get()->GetInsetsMetric(
              views::INSETS_VECTOR_IMAGE_BUTTON)));
      views::SetImageFromVectorIconWithColor(
          menu_btn_, kMoreVertIcon, kIconSize,
          views::IconColors(ui::kColorSysOnSurfaceVariant,
                            ui::kColorLabelForegroundDisabled,
                            ui::kColorSysOnSurfaceVariant));
      CreateToolbarInkdropCallbacks(menu_btn_, kColorToolbarInkDropHover,
                                    kColorToolbarInkDropRipple);
    }

    dismiss_pill_->SetPaintToLayer();
    dismiss_pill_->layer()->SetFillsBoundsOpaquely(false);
    dismiss_pill_->layer()->SetOpacity(0.0f);
  }

  void OnMouseEntered(const ui::MouseEvent& event) override {
    if (dismiss_pill_) {
      dismiss_pill_->layer()->SetOpacity(1.0f);
    }
    if (ask_pill_) {
      auto* bubble_border =
          static_cast<views::BubbleBorder*>(ask_pill_->GetBorder());
      if (bubble_border) {
        bubble_border->set_rounded_corners(
            gfx::RoundedCornersF(kCornerRadius, kCornerRadiusInner,
                                 kCornerRadiusInner, kCornerRadius));
        ask_pill_->SchedulePaint();
      }
    }
  }

  void OnMouseExited(const ui::MouseEvent& event) override {
    if (dismiss_pill_) {
      dismiss_pill_->layer()->SetOpacity(0.0f);
    }
    if (ask_pill_) {
      auto* bubble_border =
          static_cast<views::BubbleBorder*>(ask_pill_->GetBorder());
      if (bubble_border) {
        bubble_border->set_rounded_corners(gfx::RoundedCornersF(kCornerRadius));
        ask_pill_->SchedulePaint();
      }
    }
  }

  void OnThemeChanged() override {
    views::View::OnThemeChanged();
    if (GetWidget() && GetWidget()->widget_delegate()) {
      // Force a background color update by temporarily clearing it, then
      // restoring it, which bypasses the color variant equality check.
      auto* bubble_delegate =
          GetWidget()->widget_delegate()->AsBubbleDialogDelegate();
      bubble_delegate->SetBackgroundColor(ui::ColorVariant());
      bubble_delegate->SetBackgroundColor(
          ui::ColorVariant(SK_ColorTRANSPARENT));
    }
  }

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override {
    if (command_id ==
        static_cast<int>(
            GlicSelectionWidgetDelegate::MenuCommand::kHideForSite)) {
      widget_delegate_->action_delegate().OnHideForThisSite();
    } else if (command_id ==
               static_cast<int>(
                   GlicSelectionWidgetDelegate::MenuCommand::kSettings)) {
      widget_delegate_->action_delegate().OnSettings();
    }
  }

  bool IsCommandIdEnabled(int command_id) const override { return true; }

  // Non-virtual helper methods:
  void SetCopyLinkEnabled(bool enabled) {
    if (copy_link_btn_) {
      copy_link_btn_->SetEnabled(enabled);
    }
  }

  void SetPinned(bool is_pinned) {
    if (pin_btn_) {
      const gfx::VectorIcon& icon =
          is_pinned ? features::IsRoundedIconsEnabled()
                          ? vector_icons::kKeyboardArrowDownIcon
                          : vector_icons::kCaretDownOldIcon
          : features::IsRoundedIconsEnabled()
              ? vector_icons::kKeyboardArrowUpIcon
              : vector_icons::kCaretUpOldIcon;
      views::SetImageFromVectorIconWithColor(
          pin_btn_, icon, kIconSize,
          views::IconColors(ui::kColorSysOnSurfaceVariant,
                            ui::kColorLabelForegroundDisabled,
                            ui::kColorSysOnSurfaceVariant));
      pin_btn_->SetTooltipText(l10n_util::GetStringUTF16(
          is_pinned ? IDS_TAB_SEARCH_BUTTON_CXMENU_UNPIN
                    : IDS_TAB_SEARCH_BUTTON_CXMENU_PIN));
    }
  }

  void OnMenuButtonClicked() {
    if (menu_runner_ && menu_runner_->IsRunning()) {
      return;
    }
    menu_model_ = std::make_unique<GlicSelectionMenuModel>(this);
    menu_model_->AddItemWithStringId(
        static_cast<int>(
            GlicSelectionWidgetDelegate::MenuCommand::kHideForSite),
        IDS_GLIC_SELECTION_MENU_HIDE_FOR_SITE);

    auto settings_label = gfx::LocateAndRemoveAcceleratorChar(
        l10n_util::GetStringUTF16(IDS_SETTINGS), nullptr, nullptr);
    menu_model_->AddItem(
        static_cast<int>(GlicSelectionWidgetDelegate::MenuCommand::kSettings),
        settings_label);

    int visual_top_of_chip = dismiss_pill_->GetBoundsInScreen().y() +
                             dismiss_pill_->GetInsets().top();
    menu_adapter_ = std::make_unique<GlicSelectionMenuModelAdapter>(
        menu_model_.get(), this, visual_top_of_chip,
        menu_btn_->GetBoundsInScreen());
    std::unique_ptr<views::MenuItemView> menu_item =
        menu_adapter_->CreateMenu();

    menu_runner_ = std::make_unique<views::MenuRunner>(
        std::move(menu_item), views::MenuRunner::NO_FLAGS);

    gfx::Rect anchor_rect = menu_btn_->GetBoundsInScreen();
    // Offset by the top inset of the shadow in order to align the visual top
    // of the menu with the visual top of the chip.
    anchor_rect.set_y(GetBoundsInScreen().y() +
                      dismiss_pill_->GetInsets().top());
    anchor_rect.set_height(0);

    menu_runner_->RunMenuAt(GetWidget(), nullptr, anchor_rect,
                            views::MenuAnchorPosition::kTopLeft,
                            ui::mojom::MenuSourceType::kNone);
  }

 private:
  const raw_ptr<GlicSelectionWidgetDelegate> widget_delegate_;
  raw_ptr<views::ImageButton> copy_link_btn_ = nullptr;
  raw_ptr<views::ImageButton> pin_btn_ = nullptr;
  raw_ptr<views::ImageButton> menu_btn_ = nullptr;
  raw_ptr<views::BoxLayoutView> ask_pill_ = nullptr;
  raw_ptr<views::BoxLayoutView> dismiss_pill_ = nullptr;

  std::unique_ptr<GlicSelectionMenuModel> menu_model_;
  // Destruction order matters: `menu_runner_` must be destroyed before
  // `menu_adapter_` to avoid accessing a deleted delegate.
  std::unique_ptr<GlicSelectionMenuModelAdapter> menu_adapter_;
  std::unique_ptr<views::MenuRunner> menu_runner_;
};

BEGIN_METADATA(GlicSelectionContentsView)
END_METADATA

}  // namespace

GlicSelectionWidgetDelegate::GlicSelectionWidgetDelegate(
    ActionDelegate& action_delegate,
    const gfx::Rect& anchor_rect,
    const gfx::Rect& window_bounds,
    const std::u16string& selected_text,
    bool is_pinned)
    : BubbleDialogDelegate(nullptr,
                           views::BubbleBorder::BOTTOM_RIGHT,
                           views::BubbleBorder::STANDARD_SHADOW,
                           /*autosize=*/true),
      action_delegate_(action_delegate),
      original_anchor_rect_(anchor_rect),
      window_bounds_(window_bounds),
      is_pinned_(is_pinned) {
  SetContentsView(std::make_unique<GlicSelectionContentsView>(
      this, selected_text, is_pinned_));

  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  SetShowCloseButton(false);
  // Remove default dialog margins so the custom button fills the entire bubble.
  set_margins(gfx::Insets(0));
  set_corner_radius(kCornerRadius);
  SetBackgroundColor(ui::ColorVariant(SK_ColorTRANSPARENT));
  set_shadow(views::BubbleBorder::NO_SHADOW);

  UpdatePosition();
}

GlicSelectionWidgetDelegate::~GlicSelectionWidgetDelegate() = default;

void GlicSelectionWidgetDelegate::TogglePinState() {
  is_pinned_ = !is_pinned_;
  action_delegate_->OnPinToggled(is_pinned_);
  if (auto* contents_view =
          views::AsViewClass<GlicSelectionContentsView>(GetContentsView())) {
    contents_view->SetPinned(is_pinned_);
  }

  if (GetWidget() && GetWidget()->GetLayer()) {
    ui::ScopedLayerAnimationSettings settings(
        GetWidget()->GetLayer()->GetAnimator());
    settings.SetTransitionDuration(base::Milliseconds(250));
    settings.SetTweenType(gfx::Tween::EASE_OUT_2);
    UpdatePosition();
    SizeToContents();
  } else {
    UpdatePosition();
    SizeToContents();
  }
}

void GlicSelectionWidgetDelegate::UpdatePosition() {
  int total_width = GetContentsView()->GetPreferredSize().width();
  if (is_pinned_) {
    // The pill has a BubbleBorder with a STANDARD_SHADOW. This shadow adds
    // a large invisible inset (typically ~24px). We must subtract this inset
    // so the visual top of the pill aligns with our target. We also subtract
    // the default 4px arrow gap added by BubbleDialogDelegate for BOTTOM_RIGHT.
    int top_inset = 0;
    if (auto* contents_view = GetContentsView()) {
      if (!contents_view->children().empty()) {
        top_inset = contents_view->children()[0]->GetInsets().top();
      }
    }

    // Pinned to the top right area, overlapping toolbar.
    // window_bounds_ is in global screen coords.
    constexpr int kWindowEdgePadding = 16;
    int pinned_x = window_bounds_.right() - total_width - kWindowEdgePadding;

    int pinned_y = window_bounds_.y() - 8 - top_inset - 4;
    SetAnchorRect(gfx::Rect(pinned_x, pinned_y, 0, 0));
  } else {
    // Unpinned: anchored inline near selection.
    SetAnchorRect(original_anchor_rect_);
  }
}

views::ClientView* GlicSelectionWidgetDelegate::CreateClientView(
    views::Widget* widget) {
  views::ClientView* client_view =
      views::BubbleDialogDelegate::CreateClientView(widget);
  if (client_view->layer()) {
    client_view->layer()->SetFillsBoundsOpaquely(false);
  }
  return client_view;
}

void GlicSelectionWidgetDelegate::OnBeforeBubbleWidgetInit(
    views::Widget::InitParams* params,
    views::Widget* widget) const {
  params->shadow_type = views::Widget::InitParams::ShadowType::kNone;
}

void GlicSelectionWidgetDelegate::UpdateCopyLinkButton(bool enabled) {
  if (auto* contents_view =
          views::AsViewClass<GlicSelectionContentsView>(GetContentsView())) {
    contents_view->SetCopyLinkEnabled(enabled);
  }
}

void GlicSelectionWidgetDelegate::TriggerMenuCommandForTesting(int command_id) {
  if (auto* contents_view =
          views::AsViewClass<GlicSelectionContentsView>(GetContentsView())) {
    contents_view->ExecuteCommand(command_id, /*event_flags=*/0);
  }
}

}  // namespace glic
