// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/classroom/glanceables_classroom_student_view.h"

#include <array>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/glanceables/classroom/glanceables_classroom_client.h"
#include "ash/glanceables/classroom/glanceables_classroom_item_view.h"
#include "ash/glanceables/classroom/glanceables_classroom_types.h"
#include "ash/glanceables/common/glanceables_contents_scroll_view.h"
#include "ash/glanceables/common/glanceables_list_footer_view.h"
#include "ash/glanceables/common/glanceables_progress_bar_view.h"
#include "ash/glanceables/common/glanceables_view_id.h"
#include "ash/glanceables/glanceables_controller.h"
#include "ash/glanceables/glanceables_metrics.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/combobox.h"
#include "ash/style/error_message_toast.h"
#include "ash/style/typography.h"
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
#include "ui/views/controls/label.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "url/gurl.h"

namespace ash {
namespace {

// Helps to map `combobox_view_` selected index to the corresponding
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

constexpr auto kEmptyListLabelMargins = gfx::Insets::TLBR(24, 0, 32, 0);
constexpr auto kFooterMargins = gfx::Insets::TLBR(12, 2, 0, 0);

std::u16string GetAssignmentListName(size_t index) {
  CHECK(index >= 0 || index < kStudentAssignmentsListTypeOrdered.size());

  const auto iter = kStudentAssignmentsListTypeToLabel.find(
      kStudentAssignmentsListTypeOrdered[index]);
  CHECK(iter != kStudentAssignmentsListTypeToLabel.end());

  return l10n_util::GetStringUTF16(iter->second);
}

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

GlanceablesClassroomStudentView::InitParams CreateInitParamsForClassroom() {
  GlanceablesClassroomStudentView::InitParams init_params;
  init_params.context = GlanceablesClassroomStudentView::Context::kClassroom;
  init_params.combobox_model =
      std::make_unique<ClassroomStudentComboboxModel>();
  init_params.combobox_tooltip = l10n_util::GetStringUTF16(
      IDS_GLANCEABLES_CLASSROOM_DROPDOWN_ACCESSIBLE_NAME);
  init_params.expand_button_tooltip_id =
      IDS_GLANCEABLES_CLASSROOM_EXPAND_BUTTON_EXPAND_TOOLTIP;
  init_params.collapse_button_tooltip_id =
      IDS_GLANCEABLES_CLASSROOM_EXPAND_BUTTON_COLLAPSE_TOOLTIP;
  init_params.footer_title = l10n_util::GetStringUTF16(
      IDS_GLANCEABLES_LIST_FOOTER_SEE_ALL_ASSIGNMENTS_LABEL);
  init_params.footer_tooltip = l10n_util::GetStringUTF16(
      IDS_GLANCEABLES_CLASSROOM_SEE_ALL_BUTTON_ACCESSIBLE_NAME);
  init_params.header_icon = &kGlanceablesClassroomIcon;
  init_params.header_icon_tooltip_id =
      IDS_GLANCEABLES_CLASSROOM_HEADER_ICON_ACCESSIBLE_NAME;
  return init_params;
}

}  // namespace

GlanceablesClassroomStudentView::GlanceablesClassroomStudentView()
    : GlanceablesTimeManagementBubbleView(CreateInitParamsForClassroom()),
      shown_time_(base::Time::Now()) {
  const auto* const typography_provider = TypographyProvider::Get();
  empty_list_label_ = content_scroll_view()->contents()->AddChildView(
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

void GlanceablesClassroomStudentView::CancelUpdates() {
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void GlanceablesClassroomStudentView::OnHeaderIconPressed() {
  RecordClassroomHeaderIconPressed();

  OpenUrl(GURL(kClassroomHomePage));
}

void GlanceablesClassroomStudentView::OnFooterButtonPressed() {
  base::RecordAction(
      base::UserMetricsAction("Glanceables_Classroom_SeeAllPressed"));
  CHECK(combobox_view()->GetSelectedIndex());

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

void GlanceablesClassroomStudentView::SelectedListChanged() {
  SelectedAssignmentListChanged(/*initial_update=*/false);
}

void GlanceablesClassroomStudentView::AnimateResize(
    ResizeAnimation::Type resize_type) {
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
  const auto selected_index = GetComboboxSelectedIndex();
  CHECK(selected_index >= 0 ||
        selected_index < kStudentAssignmentsListTypeOrdered.size());
  selected_list_type_ = kStudentAssignmentsListTypeOrdered[selected_index];

  UpdateComboboxReplacementLabelText();

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
  progress_bar()->UpdateProgressBarVisibility(/*visible=*/true);
  combobox_view()->GetViewAccessibility().SetDescription(u"");

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

void GlanceablesClassroomStudentView::OnGetAssignments(
    const std::u16string& list_name,
    bool initial_update,
    bool success,
    std::vector<std::unique_ptr<GlanceablesClassroomAssignment>> assignments) {
  const gfx::Size old_preferred_size = GetPreferredSize();

  progress_bar()->UpdateProgressBarVisibility(/*visible=*/false);

  items_container_view()->RemoveAllChildViews();
  total_assignments_ = assignments.size();

  const size_t num_assignments = std::min(kMaxAssignments, assignments.size());
  for (size_t i = 0; i < num_assignments; ++i) {
    items_container_view()->AddChildView(
        std::make_unique<GlanceablesClassroomItemView>(
            assignments[i].get(),
            base::BindRepeating(
                &GlanceablesClassroomStudentView::OnItemViewPressed,
                base::Unretained(this), initial_update, assignments[i]->link)));
  }
  const size_t shown_assignments = items_container_view()->children().size();
  expand_button()->UpdateCounter(shown_assignments);

  const bool is_list_empty = shown_assignments == 0;
  empty_list_label_->SetVisible(is_list_empty);

  bool should_show_footer_view;
  should_show_footer_view = assignments.size() >= kMaxAssignments;
  list_footer_view()->SetVisible(should_show_footer_view);
  list_footer_view()->SetProperty(views::kMarginsKey, kFooterMargins);

  items_container_view()->GetViewAccessibility().SetName(
      l10n_util::GetStringFUTF16(
          IDS_GLANCEABLES_CLASSROOM_SELECTED_LIST_ACCESSIBLE_NAME, list_name));
  items_container_view()->NotifyAccessibilityEvent(
      ax::mojom::Event::kChildrenChanged,
      /*send_native_event=*/true);

  if (old_preferred_size != GetPreferredSize()) {
    PreferredSizeChanged();

    if (!initial_update) {
      GetWidget()->LayoutRootViewIfNecessary();
      ScrollViewToVisible();
    }
  }

  // Reset the position of the scroll view after the new data is fetched.
  content_scroll_view()->ScrollToOffset(gfx::PointF(0, 0));

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
        ErrorMessageToast::ButtonActionType::kDismiss);
  }
}

BEGIN_METADATA(GlanceablesClassroomStudentView)
END_METADATA

}  // namespace ash
