// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/classroom/glanceables_classroom_student_view.h"

#include <array>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/glanceables/classroom/glanceables_classroom_client.h"
#include "ash/glanceables/classroom/glanceables_classroom_item_view.h"
#include "ash/glanceables/classroom/glanceables_classroom_types.h"
#include "ash/glanceables/common/glanceables_contents_scroll_view.h"
#include "ash/glanceables/common/glanceables_error_message_view.h"
#include "ash/glanceables/common/glanceables_list_footer_view.h"
#include "ash/glanceables/common/glanceables_progress_bar_view.h"
#include "ash/glanceables/common/glanceables_view_id.h"
#include "ash/glanceables/glanceables_controller.h"
#include "ash/glanceables/glanceables_metrics.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/combobox.h"
#include "ash/style/counter_expand_button.h"
#include "ash/style/icon_button.h"
#include "ash/style/typography.h"
#include "ash/system/unified/glanceable_tray_child_bubble.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/metrics/user_metrics.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "base/types/cxx23_to_underlying.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/combobox_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/metadata/view_factory_internal.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "url/gurl.h"

namespace ash {
namespace {

// Helps to map `combo_box_view_` selected index to the corresponding
// `StudentAssignmentsListType` value.
constexpr std::array<StudentAssignmentsListType, 4>
    kStudentAssignmentsListTypeOrdered = {
        StudentAssignmentsListType::kAssigned,
        StudentAssignmentsListType::kNoDueDate,
        StudentAssignmentsListType::kMissing,
        StudentAssignmentsListType::kDone};

constexpr auto kStudentAssignmentsListTypeToLabel =
    base::MakeFixedFlatMap<StudentAssignmentsListType, int>(
        {{StudentAssignmentsListType::kAssigned,
          IDS_GLANCEABLES_CLASSROOM_STUDENT_DUE_SOON_LIST_NAME},
         {StudentAssignmentsListType::kNoDueDate,
          IDS_GLANCEABLES_CLASSROOM_STUDENT_NO_DUE_DATE_LIST_NAME},
         {StudentAssignmentsListType::kMissing,
          IDS_GLANCEABLES_CLASSROOM_STUDENT_MISSING_LIST_NAME},
         {StudentAssignmentsListType::kDone,
          IDS_GLANCEABLES_CLASSROOM_STUDENT_DONE_LIST_NAME}});

constexpr char kClassroomHomePage[] = "https://classroom.google.com/u/0/h";
constexpr char kClassroomWebUIAssignedUrl[] =
    "https://classroom.google.com/u/0/a/not-turned-in/all";
constexpr char kClassroomWebUIMissingUrl[] =
    "https://classroom.google.com/u/0/a/missing/all";
constexpr char kClassroomWebUIDoneUrl[] =
    "https://classroom.google.com/u/0/a/turned-in/all";

constexpr char kLastSelectedAssignmentsListPref[] =
    "ash.glanceables.classroom.student.last_selected_assignments_list";

constexpr char kExpandAnimationSmoothnessHistogramName[] =
    "Ash.Glanceables.TimeManagement.Classroom.Expand.AnimationSmoothness";
constexpr char kCollapseAnimationSmoothnessHistogramName[] =
    "Ash.Glanceables.TimeManagement.Classroom.Collapse.AnimationSmoothness";

constexpr size_t kMaxAssignments = 100;
constexpr size_t kMaxAssignmentsForV2 = 3;

constexpr auto kEmptyListLabelMargins = gfx::Insets::TLBR(24, 0, 32, 0);
constexpr auto kHeaderIconButtonMargins = gfx::Insets::TLBR(0, 0, 0, 2);
constexpr auto kViewInteriorMargins = gfx::Insets::TLBR(12, 12, 12, 12);
constexpr auto kFooterMargins = gfx::Insets::TLBR(12, 2, 0, 0);

// This should be the same value as the one in ash/style/combobox.cc
constexpr gfx::Insets kComboboxBorderInsets = gfx::Insets::TLBR(4, 10, 4, 4);

std::u16string GetAssignmentListName(size_t index) {
  CHECK(index >= 0 || index < kStudentAssignmentsListTypeOrdered.size());

  const auto iter = kStudentAssignmentsListTypeToLabel.find(
      kStudentAssignmentsListTypeOrdered[index]);
  CHECK(iter != kStudentAssignmentsListTypeToLabel.end());

  return l10n_util::GetStringUTF16(iter->second);
}

class ClassroomExpandButton : public CounterExpandButton {
  METADATA_HEADER(ClassroomExpandButton, CounterExpandButton)
 public:
  ClassroomExpandButton() = default;
  ClassroomExpandButton(const ClassroomExpandButton&) = delete;
  ClassroomExpandButton& operator=(const ClassroomExpandButton&) = delete;
  ~ClassroomExpandButton() override = default;

  std::u16string GetExpandedStateTooltipText() override {
    // The tooltip tells users that clicking on the button will collapse the
    // Classroom view.
    return l10n_util::GetStringUTF16(
        IDS_GLANCEABLES_CLASSROOM_EXPAND_BUTTON_COLLAPSE_TOOLTIP);
  }
  std::u16string GetCollapsedStateTooltipText() override {
    // The tooltip tells users that clicking on the button will expand the
    // Classroom view.
    return l10n_util::GetStringUTF16(
        IDS_GLANCEABLES_CLASSROOM_EXPAND_BUTTON_EXPAND_TOOLTIP);
  }
};

BEGIN_METADATA(ClassroomExpandButton)
END_METADATA

class ClassroomStudentComboboxModel : public ui::ComboboxModel {
 public:
  ClassroomStudentComboboxModel() = default;
  ClassroomStudentComboboxModel(const ClassroomStudentComboboxModel&) = delete;
  ClassroomStudentComboboxModel& operator=(
      const ClassroomStudentComboboxModel&) = delete;
  ~ClassroomStudentComboboxModel() override = default;

  size_t GetItemCount() const override {
    return kStudentAssignmentsListTypeOrdered.size();
  }

  std::u16string GetItemAt(size_t index) const override {
    return GetAssignmentListName(index);
  }

  std::optional<size_t> GetDefaultIndex() const override {
    const auto selected_list_type = static_cast<StudentAssignmentsListType>(
        Shell::Get()->session_controller()->GetActivePrefService()->GetInteger(
            kLastSelectedAssignmentsListPref));
    const auto iter = base::ranges::find(kStudentAssignmentsListTypeOrdered,
                                         selected_list_type);
    return iter != kStudentAssignmentsListTypeOrdered.end()
               ? iter - kStudentAssignmentsListTypeOrdered.begin()
               : 0;
  }
};

}  // namespace

GlanceablesClassroomStudentView::GlanceablesClassroomStudentView()
    : shown_time_(base::Time::Now()) {
  SetInteriorMargin(kViewInteriorMargins);
  SetOrientation(views::LayoutOrientation::kVertical);

  auto* header_container =
      AddChildView(std::make_unique<views::FlexLayoutView>());
  header_container->SetMainAxisAlignment(views::LayoutAlignment::kStart);
  header_container->SetCrossAxisAlignment(views::LayoutAlignment::kCenter);
  header_container->SetOrientation(views::LayoutOrientation::kHorizontal);

  header_view_ =
      header_container->AddChildView(std::make_unique<views::FlexLayoutView>());
  header_view_->SetCrossAxisAlignment(views::LayoutAlignment::kCenter);
  header_view_->SetOrientation(views::LayoutOrientation::kHorizontal);
  header_view_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                               views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kUnbounded)
          .WithWeight(1));

  auto* const header_icon =
      header_view_->AddChildView(std::make_unique<IconButton>(
          base::BindRepeating(
              &GlanceablesClassroomStudentView::OnHeaderIconPressed,
              base::Unretained(this)),
          IconButton::Type::kSmall, &kGlanceablesClassroomIcon,
          IDS_GLANCEABLES_CLASSROOM_HEADER_ICON_ACCESSIBLE_NAME));
  header_icon->SetBackgroundColor(SK_ColorTRANSPARENT);
  header_icon->SetProperty(views::kMarginsKey, kHeaderIconButtonMargins);
  header_icon->SetID(
      base::to_underlying(GlanceablesViewId::kClassroomBubbleHeaderIcon));

  combo_box_view_ = header_view_->AddChildView(std::make_unique<Combobox>(
      std::make_unique<ClassroomStudentComboboxModel>()));
  combo_box_view_->SetID(
      base::to_underlying(GlanceablesViewId::kClassroomBubbleComboBox));
  combo_box_view_->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_GLANCEABLES_CLASSROOM_DROPDOWN_ACCESSIBLE_NAME));
  combo_box_view_->GetViewAccessibility().SetDescription(u"");
  combo_box_view_->SetSelectionChangedCallback(base::BindRepeating(
      &GlanceablesClassroomStudentView::SelectedAssignmentListChanged,
      base::Unretained(this),
      /*initial_update=*/false));
  combobox_view_observation_.Observe(combo_box_view_);

  auto text_on_combobox = combo_box_view_->GetTextForRow(
      combo_box_view_->GetSelectedIndex().value());
  combobox_replacement_label_ = header_view_->AddChildView(
      std::make_unique<views::Label>(text_on_combobox));
  combobox_replacement_label_->SetProperty(views::kMarginsKey,
                                           kComboboxBorderInsets);
  combobox_replacement_label_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kPreferred));
  combobox_replacement_label_->SetHorizontalAlignment(
      gfx::HorizontalAlignment::ALIGN_LEFT);
  TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosTitle1,
                                        *combobox_replacement_label_);
  combobox_replacement_label_->SetAutoColorReadabilityEnabled(false);
  combobox_replacement_label_->SetEnabledColorId(
      cros_tokens::kCrosSysOnSurface);
  combobox_replacement_label_->SetVisible(false);

  expand_button_ =
      header_container->AddChildView(std::make_unique<ClassroomExpandButton>());
  expand_button_->SetID(
      base::to_underlying(GlanceablesViewId::kClassroomBubbleExpandButton));
  // This is only set visible when both Tasks and Classroom exist, where the
  // elevated background is created in that case.
  expand_button_->SetVisible(false);
  expand_button_->SetCallback(
      base::BindRepeating(&GlanceablesClassroomStudentView::ToggleExpandState,
                          base::Unretained(this)));

  if (features::AreGlanceablesV2Enabled()) {
    // TODO(b/338917100): Remove body_container_ if GlanceablesV2 flag is
    // removed, as we only need `content_scroll_view_` after that.
    body_container_ = AddChildView(std::make_unique<views::FlexLayoutView>());
  } else {
    content_scroll_view_ = AddChildView(
        std::make_unique<GlanceablesContentsScrollView>(Context::kClassroom));
    body_container_ = content_scroll_view_->SetContents(
        std::make_unique<views::FlexLayoutView>());
  }
  body_container_->SetOrientation(views::LayoutOrientation::kVertical);

  progress_bar_ = body_container_->AddChildView(
      std::make_unique<GlanceablesProgressBarView>());
  progress_bar_->UpdateProgressBarVisibility(/*visible=*/false);

  list_container_view_ =
      body_container_->AddChildView(std::make_unique<views::BoxLayoutView>());
  list_container_view_->SetID(
      base::to_underlying(GlanceablesViewId::kClassroomBubbleListContainer));
  list_container_view_->SetOrientation(
      views::BoxLayout::Orientation::kVertical);
  list_container_view_->SetBetweenChildSpacing(4);
  list_container_view_->GetViewAccessibility().SetRole(ax::mojom::Role::kList);

  const auto* const typography_provider = TypographyProvider::Get();
  empty_list_label_ = body_container_->AddChildView(
      views::Builder<views::Label>()
          .SetProperty(views::kMarginsKey, kEmptyListLabelMargins)
          .SetEnabledColorId(cros_tokens::kCrosSysOnSurface)
          .SetFontList(typography_provider->ResolveTypographyToken(
              TypographyToken::kCrosButton2))
          .SetLineHeight(typography_provider->ResolveLineHeight(
              TypographyToken::kCrosButton2))
          .SetID(base::to_underlying(
              GlanceablesViewId::kClassroomBubbleEmptyListLabel))
          .Build());

  list_footer_view_ =
      body_container_->AddChildView(std::make_unique<GlanceablesListFooterView>(
          l10n_util::GetStringUTF16(
              IDS_GLANCEABLES_LIST_FOOTER_SEE_ALL_ASSIGNMENTS_LABEL),
          l10n_util::GetStringUTF16(
              IDS_GLANCEABLES_CLASSROOM_SEE_ALL_BUTTON_ACCESSIBLE_NAME),
          base::BindRepeating(&GlanceablesClassroomStudentView::OnSeeAllPressed,
                              base::Unretained(this))));
  list_footer_view_->SetID(
      base::to_underlying(GlanceablesViewId::kClassroomBubbleListFooter));
  list_footer_view_->SetVisible(false);

  SelectedAssignmentListChanged(/*initial_update=*/true);
}

GlanceablesClassroomStudentView::~GlanceablesClassroomStudentView() {
  if (list_shown_start_time_.has_value()) {
    RecordStudentAssignmentListShowTime(
        selected_list_type_,
        base::TimeTicks::Now() - list_shown_start_time_.value(),
        /*default_list=*/selected_list_change_count_ == 0);
  }
  if (first_assignment_list_shown_) {
    RecordStudentSelectedListChangeCount(selected_list_change_count_);
  }

  RecordTotalShowTimeForClassroom(base::Time::Now() - shown_time_);
}

// static
void GlanceablesClassroomStudentView::RegisterUserProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(
      kLastSelectedAssignmentsListPref,
      base::to_underlying(StudentAssignmentsListType::kAssigned));
}

// static
void GlanceablesClassroomStudentView::ClearUserStatePrefs(
    PrefService* pref_service) {
  pref_service->ClearPref(kLastSelectedAssignmentsListPref);
}

void GlanceablesClassroomStudentView::OnViewFocused(views::View* view) {
  CHECK_EQ(view, combo_box_view_);

  AnnounceListStateOnComboBoxAccessibility();
}

bool GlanceablesClassroomStudentView::IsExpanded() const {
  return is_expanded_;
}

int GlanceablesClassroomStudentView::GetCollapsedStatePreferredHeight() const {
  return GetInteriorMargin().height() + header_view_->height();
}

void GlanceablesClassroomStudentView::CancelUpdates() {
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void GlanceablesClassroomStudentView::CreateElevatedBackground() {
  SetBackground(views::CreateThemedRoundedRectBackground(
      cros_tokens::kCrosSysSystemOnBaseOpaque, 16.f));
  list_footer_view_->SetVisible(false);
  expand_button_->SetVisible(true);
  expand_button_->SetExpanded(is_expanded_);

  if (content_scroll_view_) {
    content_scroll_view_->SetOnOverscrollCallback(
        base::BindRepeating(&GlanceablesClassroomStudentView::SetExpandState,
                            base::Unretained(this), /*is_expanded=*/false));
  }
}

void GlanceablesClassroomStudentView::SetExpandState(bool is_expanded) {
  if (is_expanded_ == is_expanded) {
    return;
  }

  is_expanded_ = is_expanded;
  expand_button_->SetExpanded(is_expanded);

  if (content_scroll_view_) {
    content_scroll_view_->SetVisible(is_expanded_);
  }
  body_container_->SetVisible(is_expanded_);
  combo_box_view_->SetVisible(is_expanded_);
  combobox_replacement_label_->SetVisible(!is_expanded_);

  for (auto& observer : observers_) {
    observer.OnExpandStateChanged(Context::kClassroom, is_expanded_);
  }

  AnimateResize();
}

void GlanceablesClassroomStudentView::ToggleExpandState() {
  SetExpandState(!is_expanded_);
}

void GlanceablesClassroomStudentView::OnSeeAllPressed() {
  base::RecordAction(
      base::UserMetricsAction("Glanceables_Classroom_SeeAllPressed"));
  CHECK(combo_box_view_->GetSelectedIndex());

  switch (selected_list_type_) {
    case StudentAssignmentsListType::kAssigned:
    case StudentAssignmentsListType::kNoDueDate:
      return OpenUrl(GURL(kClassroomWebUIAssignedUrl));
    case StudentAssignmentsListType::kMissing:
      return OpenUrl(GURL(kClassroomWebUIMissingUrl));
    case StudentAssignmentsListType::kDone:
      return OpenUrl(GURL(kClassroomWebUIDoneUrl));
  }
}

void GlanceablesClassroomStudentView::OpenUrl(const GURL& url) const {
  NewWindowDelegate::GetPrimary()->OpenUrl(
      url, NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      NewWindowDelegate::Disposition::kNewForegroundTab);
}

void GlanceablesClassroomStudentView::OnItemViewPressed(
    bool initial_list_selected,
    const GURL& url) {
  RecordStudentAssignmentPressed(/*default_list=*/initial_list_selected);

  OpenUrl(url);
}

void GlanceablesClassroomStudentView::OnHeaderIconPressed() {
  RecordClassroomHeaderIconPressed();

  OpenUrl(GURL(kClassroomHomePage));
}

void GlanceablesClassroomStudentView::SelectedAssignmentListChanged(
    bool initial_update) {
  auto* const client =
      Shell::Get()->glanceables_controller()->GetClassroomClient();
  if (!client) {
    // Hide this bubble when no classroom client exists.
    SetVisible(false);
    return;
  }

  const auto prev_selected_list_type = selected_list_type_;
  CHECK(combo_box_view_->GetSelectedIndex());
  const auto selected_index = combo_box_view_->GetSelectedIndex().value();
  CHECK(selected_index >= 0 ||
        selected_index < kStudentAssignmentsListTypeOrdered.size());
  selected_list_type_ = kStudentAssignmentsListTypeOrdered[selected_index];

  combobox_replacement_label_->SetText(
      combo_box_view_->GetTextForRow(selected_index));

  if (!initial_update) {
    base::RecordAction(
        base::UserMetricsAction("Glanceables_Classroom_SelectedListChanged"));
    if (list_shown_start_time_.has_value()) {
      RecordStudentAssignmentListShowTime(
          prev_selected_list_type,
          base::TimeTicks::Now() - list_shown_start_time_.value(),
          /*default_list=*/selected_list_change_count_ == 0);
    }
    RecordStudentAssignmentListSelected(selected_list_type_);
    selected_list_change_count_++;
  }
  list_shown_start_time_.reset();

  Shell::Get()->session_controller()->GetActivePrefService()->SetInteger(
      kLastSelectedAssignmentsListPref,
      base::to_underlying(selected_list_type_));

  // Cancel any old pending assignment requests.
  CancelUpdates();

  assignments_requested_time_ = base::TimeTicks::Now();
  progress_bar_->UpdateProgressBarVisibility(/*visible=*/true);
  combo_box_view_->GetViewAccessibility().SetDescription(u"");

  auto callback =
      base::BindOnce(&GlanceablesClassroomStudentView::OnGetAssignments,
                     weak_ptr_factory_.GetWeakPtr(),
                     GetAssignmentListName(selected_index), initial_update);
  switch (selected_list_type_) {
    case StudentAssignmentsListType::kAssigned:
      empty_list_label_->SetText(l10n_util::GetStringUTF16(
          IDS_GLANCEABLES_CLASSROOM_STUDENT_EMPTY_ITEM_DUE_LIST));
      return client->GetStudentAssignmentsWithApproachingDueDate(
          std::move(callback));
    case StudentAssignmentsListType::kNoDueDate:
      empty_list_label_->SetText(l10n_util::GetStringUTF16(
          IDS_GLANCEABLES_CLASSROOM_STUDENT_EMPTY_ITEM_DUE_LIST));
      return client->GetStudentAssignmentsWithoutDueDate(std::move(callback));
    case StudentAssignmentsListType::kMissing:
      empty_list_label_->SetText(l10n_util::GetStringUTF16(
          IDS_GLANCEABLES_CLASSROOM_STUDENT_EMPTY_ITEM_MISSING_LIST));
      return client->GetStudentAssignmentsWithMissedDueDate(
          std::move(callback));
    case StudentAssignmentsListType::kDone:
      empty_list_label_->SetText(l10n_util::GetStringUTF16(
          IDS_GLANCEABLES_CLASSROOM_STUDENT_EMPTY_ITEM_DONE_LIST));
      return client->GetCompletedStudentAssignments(std::move(callback));
  }
}

void GlanceablesClassroomStudentView::AnimateResize() {
  const int current_height = size().height();
  if (current_height == 0) {
    return;
  }
  resize_animation_.reset();

  if (!ui::ScopedAnimationDurationScaleMode::duration_multiplier()) {
    PreferredSizeChanged();
    return;
  }

  // Check if the available height is large enough for the preferred height, so
  // that the target height for the animation is correctly bounded.
  const views::SizeBound available_height =
      parent()->GetAvailableSize(this).height();
  const int preferred_height = GetPreferredSize().height();
  const int target_height =
      available_height.is_bounded()
          ? std::min(available_height.value(), preferred_height)
          : preferred_height;
  if (current_height == target_height) {
    return;
  }

  SetUpResizeThroughputTracker(target_height > current_height
                                   ? kExpandAnimationSmoothnessHistogramName
                                   : kCollapseAnimationSmoothnessHistogramName);
  resize_animation_ = std::make_unique<ResizeAnimation>(
      current_height, target_height, this,
      ResizeAnimation::Type::kContainerExpandStateChanged);
  resize_animation_->Start();
}

void GlanceablesClassroomStudentView::OnGetAssignments(
    const std::u16string& list_name,
    bool initial_update,
    bool success,
    std::vector<std::unique_ptr<GlanceablesClassroomAssignment>> assignments) {
  const gfx::Size old_preferred_size = GetPreferredSize();

  progress_bar_->UpdateProgressBarVisibility(/*visible=*/false);

  list_container_view_->RemoveAllChildViews();
  total_assignments_ = assignments.size();

  const size_t num_assignments =
      features::AreGlanceablesV2Enabled()
          ? std::min(kMaxAssignmentsForV2, assignments.size())
          : std::min(kMaxAssignments, assignments.size());
  for (size_t i = 0; i < num_assignments; ++i) {
    list_container_view_->AddChildView(
        std::make_unique<GlanceablesClassroomItemView>(
            assignments[i].get(),
            base::BindRepeating(
                &GlanceablesClassroomStudentView::OnItemViewPressed,
                base::Unretained(this), initial_update, assignments[i]->link)));
  }
  const size_t shown_assignments = list_container_view_->children().size();
  // TODO(b/338917100): Revisit the counter used on the expand button later to
  // see if we want to use the shown one or the total one.
  expand_button_->UpdateCounter(shown_assignments);

  const bool is_list_empty = shown_assignments == 0;
  empty_list_label_->SetVisible(is_list_empty);

  bool should_show_footer_view;
  if (features::AreGlanceablesV2Enabled()) {
    should_show_footer_view = !is_list_empty;
  } else {
    should_show_footer_view = assignments.size() >= kMaxAssignments;
  }
  list_footer_view_->SetVisible(should_show_footer_view);
  list_footer_view_->SetProperty(views::kMarginsKey, kFooterMargins);

  list_container_view_->GetViewAccessibility().SetName(
      l10n_util::GetStringFUTF16(
          IDS_GLANCEABLES_CLASSROOM_SELECTED_LIST_ACCESSIBLE_NAME, list_name));
  list_container_view_->NotifyAccessibilityEvent(
      ax::mojom::Event::kChildrenChanged,
      /*send_native_event=*/true);

  // The list is shown in response to the action on the assignment selector
  // combobox, notify the user of the list state id the combox is still focused.
  AnnounceListStateOnComboBoxAccessibility();

  if (old_preferred_size != GetPreferredSize()) {
    PreferredSizeChanged();

    if (!initial_update) {
      GetWidget()->LayoutRootViewIfNecessary();
      ScrollViewToVisible();
    }
  }

  auto* controller = Shell::Get()->glanceables_controller();

  if (initial_update) {
    RecordClassromInitialLoadTime(
        /*first_occurrence=*/controller->bubble_shown_count() == 1,
        base::TimeTicks::Now() - controller->last_bubble_show_time());
  } else {
    RecordClassroomChangeLoadTime(
        success, base::TimeTicks::Now() - assignments_requested_time_);
  }

  list_shown_start_time_ = base::TimeTicks::Now();
  first_assignment_list_shown_ = true;

  if (success) {
    MaybeDismissErrorMessage();
  } else {
    ShowErrorMessage(
        l10n_util::GetStringUTF16(IDS_GLANCEABLES_CLASSROOM_FETCH_ERROR),
        base::BindRepeating(
            &GlanceablesClassroomStudentView::MaybeDismissErrorMessage,
            base::Unretained(this)),
        GlanceablesErrorMessageView::ButtonActionType::kDismiss);
    error_message()->SetProperty(views::kViewIgnoredByLayoutKey, true);
  }
}

// TODO(b/338917100): Remove this along with the view observer as the
// announcement is not needed anymore.
void GlanceablesClassroomStudentView::
    AnnounceListStateOnComboBoxAccessibility() {
  if (empty_list_label_->GetVisible()) {
    combo_box_view_->GetViewAccessibility().AnnounceText(
        empty_list_label_->GetText());
  } else if (list_footer_view_->title_label()->GetVisible()) {
    combo_box_view_->GetViewAccessibility().AnnounceText(
        list_footer_view_->title_label()->GetText());
  }
}

BEGIN_METADATA(GlanceablesClassroomStudentView)
END_METADATA

}  // namespace ash
