// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_answers/ui/quick_answers_view.h"

#include "ash/public/cpp/assistant/assistant_interface_binder.h"
#include "ash/quick_answers/quick_answers_ui_controller.h"
#include "ash/quick_answers/ui/quick_answers_pre_target_handler.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/painter.h"
#include "ui/views/widget/tooltip_manager.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {
namespace {

using chromeos::quick_answers::QuickAnswer;
using chromeos::quick_answers::QuickAnswerText;
using chromeos::quick_answers::QuickAnswerUiElement;
using chromeos::quick_answers::QuickAnswerUiElementType;
using views::Button;
using views::Label;
using views::View;

// Spacing between this view and the anchor view.
constexpr int kMarginDip = 10;

constexpr gfx::Insets kMainViewInsets(4, 0);
constexpr gfx::Insets kContentViewInsets(8, 0, 8, 16);
constexpr float kHoverStateAlpha = 0.06f;
constexpr int kMaxRows = 3;

// Assistant icon.
constexpr int kAssistantIconSizeDip = 16;
constexpr gfx::Insets kAssistantIconInsets(10, 10, 0, 8);

// Spacing between lines in the main view.
constexpr int kLineSpacingDip = 4;
constexpr int kLineHeightDip = 20;

// Spacing between labels in the horizontal elements view.
constexpr int kLabelSpacingDip = 2;

// TODO(llin): Move to grd after confirming specs (b/149758492).
constexpr char kDefaultLoadingStr[] = "Loading...";
constexpr char kDefaultRetryStr[] = "Retry";
constexpr char kNetworkErrorStr[] = "Cannot connect to internet.";

// Dogfood button.
constexpr int kDogfoodButtonMarginDip = 4;
constexpr int kDogfoodButtonSizeDip = 20;
constexpr SkColor kDogfoodButtonColor = gfx::kGoogleGrey500;

// Accessibility.
// TODO(siabhijeet): Move to grd (tracked in b/149758492).
constexpr char kA11yAlertAnnouncement[] =
    "Info related to your selection available. Use Up arrow key to access.";
constexpr char kA11yNameText[] = "Info related to your selection";
constexpr char kA11yDescTemplate[] =
    "%s; Click the dialog to see result in Assistant.";
constexpr char kA11yRetryLabelNameTemplate[] = "%s: Retry";
constexpr char kA11yRetryLabelDesc[] =
    "Cannot connect to the internet. Click to try again.";

// Maximum height QuickAnswersView can expand to.
int MaximumViewHeight() {
  return kMainViewInsets.height() + kContentViewInsets.height() +
         kMaxRows * kLineHeightDip + (kMaxRows - 1) * kLineSpacingDip;
}

// Adds |text_element| as label to the container.
Label* AddTextElement(const QuickAnswerText& text_element, View* container) {
  auto* label =
      container->AddChildView(std::make_unique<Label>(text_element.text));
  label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  label->SetEnabledColor(text_element.color);
  label->SetLineHeight(kLineHeightDip);
  return label;
}

// Adds the list of |QuickAnswerUiElement| horizontally to the container.
View* AddHorizontalUiElements(
    const std::vector<std::unique_ptr<QuickAnswerUiElement>>& elements,
    View* container) {
  auto* labels_container =
      container->AddChildView(std::make_unique<views::View>());
  labels_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
      kLabelSpacingDip));

  for (const auto& element : elements) {
    switch (element->type) {
      case QuickAnswerUiElementType::kText:
        AddTextElement(*static_cast<QuickAnswerText*>(element.get()),
                       labels_container);
        break;
      case QuickAnswerUiElementType::kImage:
        // TODO(yanxiao): Add image view
        break;
      default:
        break;
    }
  }

  return labels_container;
}

}  // namespace

// QuickAnswersView -----------------------------------------------------------

QuickAnswersView::QuickAnswersView(const gfx::Rect& anchor_view_bounds,
                                   const std::string& title,
                                   QuickAnswersUiController* controller)
    : Button(this),
      anchor_view_bounds_(anchor_view_bounds),
      controller_(controller),
      title_(title),
      quick_answers_view_handler_(
          std::make_unique<QuickAnswersPreTargetHandler>(this)),
      focus_search_(std::make_unique<QuickAnswersFocusSearch>(
          this,
          base::BindRepeating(&QuickAnswersView::GetFocusableViews,
                              base::Unretained(this)))) {
  InitLayout();
  InitWidget();

  // Focus.
  SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  SetInstallFocusRingOnFocus(false);

  // This is because waiting for mouse-release to fire buttons would be too
  // late, since mouse-press dismisses the menu.
  SetButtonNotifyActionToOnPress(this);

  // Allow tooltips to be shown despite menu-controller owning capture.
  GetWidget()->SetNativeWindowProperty(
      views::TooltipManager::kGroupingPropertyKey,
      reinterpret_cast<void*>(views::MenuConfig::kMenuControllerGroupingId));
}

QuickAnswersView::~QuickAnswersView() = default;

const char* QuickAnswersView::GetClassName() const {
  return "QuickAnswersView";
}

void QuickAnswersView::OnFocus() {
  SetBackgroundState(true);
  View* wants_focus = focus_search_->FindNextFocusableView(
      nullptr, views::FocusSearch::SearchDirection::kForwards,
      views::FocusSearch::TraversalDirection::kDown,
      views::FocusSearch::StartingViewPolicy::kCheckStartingView,
      views::FocusSearch::AnchoredDialogPolicy::kSkipAnchoredDialog, nullptr,
      nullptr);
  if (wants_focus != this)
    wants_focus->RequestFocus();
  else
    NotifyAccessibilityEvent(ax::mojom::Event::kFocus, true);
}

void QuickAnswersView::OnBlur() {
  SetBackgroundState(false);
}

views::FocusTraversable* QuickAnswersView::GetPaneFocusTraversable() {
  return focus_search_.get();
}

void QuickAnswersView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  // The view itself is not focused for retry-mode, so should not be announced
  // by the screen reader.
  if (retry_label_) {
    node_data->role = ax::mojom::Role::kNone;
    node_data->SetName(std::string());
    node_data->SetDescription(std::string());
    return;
  }

  node_data->role = ax::mojom::Role::kDialog;
  node_data->SetName(kA11yNameText);
}

std::vector<views::View*> QuickAnswersView::GetFocusableViews() {
  std::vector<views::View*> focusable_views;
  // The view itself does not gain focus for retry-view and transfers it to the
  // retry-label, and so is not included when this is the case.
  if (!retry_label_)
    focusable_views.push_back(this);
  if (retry_label_ && retry_label_->GetVisible())
    focusable_views.push_back(retry_label_);
  if (dogfood_button_ && dogfood_button_->GetVisible())
    focusable_views.push_back(dogfood_button_);
  return focusable_views;
}

void QuickAnswersView::StateChanged(views::Button::ButtonState old_state) {
  Button::StateChanged(old_state);
  const bool hovered = GetState() == Button::STATE_HOVERED;
  if (hovered || (GetState() == Button::STATE_NORMAL))
    SetBackgroundState(hovered);
}

void QuickAnswersView::ButtonPressed(views::Button* sender,
                                     const ui::Event& event) {
  if (sender == dogfood_button_) {
    controller_->OnDogfoodButtonPressed();
    return;
  }
  if (sender == retry_label_) {
    controller_->OnRetryLabelPressed();
    return;
  }
  if (sender == this) {
    SendQuickAnswersQuery();
    return;
  }
}

void QuickAnswersView::SetButtonNotifyActionToOnPress(views::Button* button) {
  DCHECK(button);
  button->button_controller()->set_notify_action(
      views::ButtonController::NotifyAction::kOnPress);
}

void QuickAnswersView::SendQuickAnswersQuery() {
  controller_->OnQuickAnswersViewPressed();
}

void QuickAnswersView::UpdateAnchorViewBounds(
    const gfx::Rect& anchor_view_bounds) {
  anchor_view_bounds_ = anchor_view_bounds;
  UpdateBounds();
}

void QuickAnswersView::UpdateView(const gfx::Rect& anchor_view_bounds,
                                  const QuickAnswer& quick_answer) {
  has_second_row_answer_ = !quick_answer.second_answer_row.empty();
  anchor_view_bounds_ = anchor_view_bounds;
  retry_label_ = nullptr;

  UpdateQuickAnswerResult(quick_answer);
  UpdateBounds();
}

void QuickAnswersView::ShowRetryView() {
  if (retry_label_)
    return;

  ResetContentView();
  main_view_->SetBackground(views::CreateSolidBackground(SK_ColorTRANSPARENT));

  // Add title.
  AddTextElement({title_}, content_view_);

  // Add error label.
  std::vector<std::unique_ptr<QuickAnswerUiElement>> description_labels;
  description_labels.push_back(
      std::make_unique<QuickAnswerText>(kNetworkErrorStr, gfx::kGoogleGrey700));
  auto* description_container =
      AddHorizontalUiElements(description_labels, content_view_);

  // Add retry label.
  retry_label_ =
      description_container->AddChildView(std::make_unique<views::LabelButton>(
          /*listener=*/this, base::UTF8ToUTF16(kDefaultRetryStr)));
  retry_label_->SetEnabledTextColors(gfx::kGoogleBlue600);
  retry_label_->SetFocusForPlatform();
  retry_label_->SetRequestFocusOnPress(true);
  SetButtonNotifyActionToOnPress(retry_label_);
  retry_label_->SetAccessibleName(base::UTF8ToUTF16(
      base::StringPrintf(kA11yRetryLabelNameTemplate, kA11yNameText)));
  retry_label_->GetViewAccessibility().OverrideDescription(kA11yRetryLabelDesc);
}

void QuickAnswersView::AddAssistantIcon() {
  // Add Assistant icon.
  auto* assistant_icon =
      main_view_->AddChildView(std::make_unique<views::ImageView>());
  assistant_icon->SetBorder(views::CreateEmptyBorder(kAssistantIconInsets));
  assistant_icon->SetImage(gfx::CreateVectorIcon(
      chromeos::kAssistantIcon, kAssistantIconSizeDip, gfx::kPlaceholderColor));
}

void QuickAnswersView::AddDogfoodButton() {
  auto* dogfood_view = AddChildView(std::make_unique<View>());
  auto* layout =
      dogfood_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical,
          gfx::Insets(kDogfoodButtonMarginDip)));
  layout->set_cross_axis_alignment(views::BoxLayout::CrossAxisAlignment::kEnd);
  auto dogfood_button = std::make_unique<views::ImageButton>(/*listener=*/this);
  dogfood_button->SetImage(
      views::Button::ButtonState::STATE_NORMAL,
      gfx::CreateVectorIcon(kDogfoodIcon, kDogfoodButtonSizeDip,
                            kDogfoodButtonColor));
  dogfood_button->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_ASH_QUICK_ANSWERS_DOGFOOD_BUTTON_TOOLTIP_TEXT));
  dogfood_button_ = dogfood_view->AddChildView(std::move(dogfood_button));
  SetButtonNotifyActionToOnPress(dogfood_button_);
}

void QuickAnswersView::InitLayout() {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  SetBackground(views::CreateSolidBackground(SK_ColorWHITE));

  main_view_ = AddChildView(std::make_unique<View>());
  auto* layout =
      main_view_->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, kMainViewInsets));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);

  // Add Assistant icon.
  AddAssistantIcon();

  // Add content view.
  content_view_ = main_view_->AddChildView(std::make_unique<View>());
  content_view_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, kContentViewInsets,
      kLineSpacingDip));
  AddTextElement({title_}, content_view_);
  AddTextElement({kDefaultLoadingStr, gfx::kGoogleGrey700}, content_view_);

  // Add dogfood button, if in dogfood.
  if (chromeos::features::IsQuickAnswersDogfood())
    AddDogfoodButton();
}

void QuickAnswersView::InitWidget() {
  views::Widget::InitParams params;
  params.activatable = views::Widget::InitParams::Activatable::ACTIVATABLE_NO;
  params.shadow_elevation = 2;
  params.shadow_type = views::Widget::InitParams::ShadowType::kDrop;
  params.type = views::Widget::InitParams::TYPE_POPUP;
  params.z_order = ui::ZOrderLevel::kFloatingUIElement;

  // Parent the widget depending on the context.
  auto* active_menu_controller = views::MenuController::GetActiveInstance();
  if (active_menu_controller && active_menu_controller->owner()) {
    params.parent = active_menu_controller->owner()->GetNativeView();
    params.child = true;
  } else {
    params.context = Shell::Get()->GetRootWindowForNewWindows();
  }

  views::Widget* widget = new views::Widget();
  widget->Init(std::move(params));
  widget->SetContentsView(this);
  UpdateBounds();
}

void QuickAnswersView::UpdateBounds() {
  int desired_width = anchor_view_bounds_.width();

  // Multi-line labels need to be resized to be compatible with |desired_width|.
  if (first_answer_label_) {
    int label_desired_width =
        desired_width - kMainViewInsets.width() - kContentViewInsets.width() -
        kAssistantIconInsets.width() - kAssistantIconSizeDip;
    first_answer_label_->SizeToFit(label_desired_width);
  }

  int height = GetHeightForWidth(desired_width);
  int y = anchor_view_bounds_.y() - kMarginDip - height;

  // Reserve space at the top since the view might expand for two-line answers.
  int y_min = anchor_view_bounds_.y() - kMarginDip - MaximumViewHeight();
  if (y_min < display::Screen::GetScreen()
                  ->GetDisplayMatching(anchor_view_bounds_)
                  .bounds()
                  .y()) {
    // The Quick Answers view will be off screen if showing above the anchor.
    // Show below the anchor instead.
    y = anchor_view_bounds_.bottom() + kMarginDip;
  }

  gfx::Rect bounds = {{anchor_view_bounds_.x(), y}, {desired_width, height}};
  wm::ConvertRectFromScreen(GetWidget()->GetNativeWindow()->parent(), &bounds);
  GetWidget()->SetBounds(bounds);
}

void QuickAnswersView::UpdateQuickAnswerResult(
    const QuickAnswer& quick_answer) {
  // Check if the view (or any of its children) had focus before resetting the
  // view, so it can be restored for the updated view.
  bool pane_already_had_focus = Contains(GetFocusManager()->GetFocusedView());
  ResetContentView();

  // Add title.
  AddHorizontalUiElements(quick_answer.title, content_view_);

  // Add first row answer.
  View* first_answer_view = nullptr;
  if (!quick_answer.first_answer_row.empty()) {
    first_answer_view =
        AddHorizontalUiElements(quick_answer.first_answer_row, content_view_);
  }
  bool first_answer_is_single_label =
      first_answer_view->children().size() == 1 &&
      first_answer_view->children().front()->GetClassName() ==
          views::Label::kViewClassName;
  if (first_answer_is_single_label) {
    // Update answer announcement.
    auto* answer_label =
        static_cast<Label*>(first_answer_view->children().front());
    GetViewAccessibility().OverrideDescription(base::StringPrintf(
        kA11yDescTemplate, base::UTF16ToUTF8(answer_label->GetText()).c_str()));
  }

  // Add second row answer.
  if (!quick_answer.second_answer_row.empty()) {
    AddHorizontalUiElements(quick_answer.second_answer_row, content_view_);
  } else {
    // If secondary-answer does not exist and primary-answer is a single label,
    // allow that label to wrap through to the row intended for the former.
    if (first_answer_is_single_label) {
      // Cache multi-line label for resizing when view bounds change.
      first_answer_label_ =
          static_cast<Label*>(first_answer_view->children().front());
      first_answer_label_->SetMultiLine(true);
      first_answer_label_->SetMaxLines(kMaxRows - /*exclude title*/ 1);
    }
  }

  // Restore focus if the view had one prior to updating the answer.
  if (pane_already_had_focus) {
    RequestFocus();
  } else {
    // Announce that a Quick Answer is available.
    GetViewAccessibility().AnnounceText(
        base::UTF8ToUTF16(kA11yAlertAnnouncement));
  }
}

void QuickAnswersView::SetBackgroundState(bool highlight) {
  if (highlight && !retry_label_) {
    main_view_->SetBackground(views::CreateBackgroundFromPainter(
        views::Painter::CreateSolidRoundRectPainter(
            SkColorSetA(SK_ColorBLACK, kHoverStateAlpha * 0xFF),
            /*radius=*/0, kMainViewInsets)));
  } else if (!highlight) {
    main_view_->SetBackground(views::CreateSolidBackground(SK_ColorWHITE));
  }
}

void QuickAnswersView::ResetContentView() {
  content_view_->RemoveAllChildViews(true);
  first_answer_label_ = nullptr;
}

}  // namespace ash
