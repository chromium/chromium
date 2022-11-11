// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_warn_dialog.h"

#include <memory>
#include <string>
#include <utility>

#include "build/chromeos_buildflags.h"
#include "chrome/browser/ash/policy/dlp/dlp_files_controller.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_confidential_contents.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_confidential_file.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_types.h"
#include "ui/chromeos/strings/grit/ui_chromeos_strings.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/public/cpp/style/color_provider.h"
#include "ash/public/cpp/style/scoped_light_mode_as_default.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace policy {

namespace {

// The corner radius.
constexpr int kDialogCornerRadius = 12;

// The dialog insets.
constexpr auto kMarginInsets = gfx::Insets::TLBR(20, 0, 20, 0);

// The insets in the upper part of the dialog.
constexpr auto kTopPanelInsets = gfx::Insets::TLBR(0, 24, 16, 24);

// The insests in the container holding the list of confidential contents.
constexpr auto kConfidentialListInsets = gfx::Insets::TLBR(8, 24, 8, 24);

// The insets of a single confidential content row.
constexpr auto kConfidentialRowInsets = gfx::Insets::TLBR(6, 0, 6, 0);

// The spacing between the elements in a box layout.
constexpr int kBetweenChildSpacing = 16;

// The size of the managed icon.
constexpr int kManagedIconSize = 32;

// The size of the favicon.
constexpr int kFaviconSize = 20;

// The font used for in the dialog.
constexpr char kFontName[] = "Roboto";

// The font size of the text.
constexpr int kBodyFontSize = 14;

// The line height of the text.
constexpr int kBodyLineHeight = 20;

// The font size of the title.
constexpr int kTitleFontSize = 16;

// The line height of the title.
constexpr int kTitleLineHeight = 24;

// The line height of the confidential content title label.
constexpr int kConfidentialContentLineHeight = 20;

// Maximum height of the confidential content scrollable list.
// This can hold seven rows.
constexpr int kConfidentialContentListMaxHeight = 240;

// Returns the destination name for |dst_component|
const std::u16string GetDestinationComponentForFiles(
    DlpRulesManager::Component dst_component) {
  switch (dst_component) {
    case DlpRulesManager::Component::kArc:
      return l10n_util::GetStringUTF16(
          IDS_FILE_BROWSER_ANDROID_FILES_ROOT_LABEL);
    case DlpRulesManager::Component::kCrostini:
      return l10n_util::GetStringUTF16(IDS_FILE_BROWSER_LINUX_FILES_ROOT_LABEL);
    case DlpRulesManager::Component::kPluginVm:
      return l10n_util::GetStringUTF16(
          IDS_FILE_BROWSER_PLUGIN_VM_DIRECTORY_LABEL);
    case DlpRulesManager::Component::kUsb:
      return l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_FILES_DESTINATION_REMOVABLE_STORAGE);
    case DlpRulesManager::Component::kDrive:
      return l10n_util::GetStringUTF16(IDS_FILE_BROWSER_DRIVE_DIRECTORY_LABEL);
    case DlpRulesManager::Component::kUnknownComponent:
      NOTREACHED();
      return u"";
  }
}

// Returns the OK button label for |files_action|.
const std::u16string GetDialogButtonOkLabelForFiles(
    DlpFilesController::FileAction files_action) {
  switch (files_action) {
    case DlpFilesController::FileAction::kDownload:
      return l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_FILES_DOWNLOAD_WARN_CONTINUE_BUTTON);
    case DlpFilesController::FileAction::kUpload:
      return l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_FILES_UPLOAD_WARN_CONTINUE_BUTTON);
    case DlpFilesController::FileAction::kCopy:
      return l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_FILES_COPY_WARN_CONTINUE_BUTTON);
    case DlpFilesController::FileAction::kMove:
      return l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_FILES_MOVE_WARN_CONTINUE_BUTTON);
    case DlpFilesController::FileAction::kOpen:
    case DlpFilesController::FileAction::kShare:
      return l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_FILES_OPEN_WARN_CONTINUE_BUTTON);
    case DlpFilesController::FileAction::kTransfer:
    case DlpFilesController::FileAction::kUnknown:
      // TODO(crbug.com/1361900): Set proper text when file action is unknown.
      return l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_FILES_TRANSFER_WARN_CONTINUE_BUTTON);
  }
}

// Returns the title for |files_action|.
const std::u16string GetTitleForFiles(
    DlpFilesController::FileAction files_action,
    int files_number) {
  switch (files_action) {
    case DlpFilesController::FileAction::kDownload:
      return l10n_util::GetPluralStringFUTF16(
          // Download action is only allowed for one file.
          IDS_POLICY_DLP_FILES_DOWNLOAD_WARN_TITLE, 1);
    case DlpFilesController::FileAction::kUpload:
      return l10n_util::GetPluralStringFUTF16(
          IDS_POLICY_DLP_FILES_UPLOAD_WARN_TITLE, files_number);
    case DlpFilesController::FileAction::kCopy:
      return l10n_util::GetPluralStringFUTF16(
          IDS_POLICY_DLP_FILES_COPY_WARN_TITLE, files_number);
    case DlpFilesController::FileAction::kMove:
      return l10n_util::GetPluralStringFUTF16(
          IDS_POLICY_DLP_FILES_MOVE_WARN_TITLE, files_number);
    case DlpFilesController::FileAction::kOpen:
    case DlpFilesController::FileAction::kShare:
      return l10n_util::GetPluralStringFUTF16(
          IDS_POLICY_DLP_FILES_OPEN_WARN_TITLE, files_number);
    case DlpFilesController::FileAction::kTransfer:
    case DlpFilesController::FileAction::kUnknown:  // TODO(crbug.com/1361900)
                                                    // Set proper text when file
                                                    // action is unknown
      return l10n_util::GetPluralStringFUTF16(
          IDS_POLICY_DLP_FILES_TRANSFER_WARN_TITLE, files_number);
  }
}

// Returns the message for |files_action|.
const std::u16string GetMessageForFiles(
    const DlpWarnDialog::DlpWarnDialogOptions& options) {
  DCHECK(options.files_action.has_value());
  std::u16string destination;
  int num_files;
  int message_id;
  switch (options.files_action.value()) {
    case DlpFilesController::FileAction::kDownload:
      DCHECK(options.destination_component.has_value());
      destination = GetDestinationComponentForFiles(
          options.destination_component.value());
      num_files = 1;  // Download action is only for one file.
      message_id = IDS_POLICY_DLP_FILES_DOWNLOAD_WARN_MESSAGE;
      break;
    case DlpFilesController::FileAction::kUpload:
      DCHECK(!options.destination_pattern->empty());
      destination = base::UTF8ToUTF16(options.destination_pattern.value());
      num_files = options.confidential_files.size();
      message_id = IDS_POLICY_DLP_FILES_UPLOAD_WARN_MESSAGE;
      break;
    case DlpFilesController::FileAction::kCopy:
      DCHECK(!options.destination_pattern->empty());
      destination = GetDestinationComponentForFiles(
          options.destination_component.value());
      num_files = options.confidential_files.size();
      message_id = IDS_POLICY_DLP_FILES_COPY_WARN_MESSAGE;
      break;
    case DlpFilesController::FileAction::kMove:
      DCHECK(!options.destination_pattern->empty());
      destination = GetDestinationComponentForFiles(
          options.destination_component.value());
      num_files = options.confidential_files.size();
      message_id = IDS_POLICY_DLP_FILES_MOVE_WARN_MESSAGE;
      break;
    case DlpFilesController::FileAction::kOpen:
    case DlpFilesController::FileAction::kShare:
      if (options.destination_component.has_value()) {
        destination = GetDestinationComponentForFiles(
            options.destination_component.value());
      } else {
        DCHECK(!options.destination_pattern->empty());
        destination = base::UTF8ToUTF16(options.destination_pattern.value());
      }
      num_files = options.confidential_files.size();
      message_id = IDS_POLICY_DLP_FILES_OPEN_WARN_MESSAGE;
      break;
    case DlpFilesController::FileAction::kTransfer:
    case DlpFilesController::FileAction::kUnknown:
      // TODO(crbug.com/1361900): Set proper text when file action is unknown
      if (options.destination_component.has_value()) {
        destination = GetDestinationComponentForFiles(
            options.destination_component.value());
      } else {
        DCHECK(!options.destination_pattern->empty());
        destination = base::UTF8ToUTF16(options.destination_pattern.value());
      }
      num_files = options.confidential_files.size();
      message_id = IDS_POLICY_DLP_FILES_TRANSFER_WARN_MESSAGE;
      break;
  }
  return base::ReplaceStringPlaceholders(
      l10n_util::GetPluralStringFUTF16(message_id, num_files), destination,
      /*offset=*/nullptr);
}

// Returns the OK button label for |restriction|.
const std::u16string GetDialogButtonOkLabel(
    DlpWarnDialog::DlpWarnDialogOptions options) {
  switch (options.restriction) {
    case DlpWarnDialog::Restriction::kScreenCapture:
      return l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_SCREEN_CAPTURE_WARN_CONTINUE_BUTTON);
    case DlpWarnDialog::Restriction::kVideoCapture:
      return l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_VIDEO_CAPTURE_WARN_CONTINUE_BUTTON);
    case DlpWarnDialog::Restriction::kPrinting:
      return l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_PRINTING_WARN_CONTINUE_BUTTON);
    case DlpWarnDialog::Restriction::kScreenShare:
      return l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_SCREEN_SHARE_WARN_CONTINUE_BUTTON);
    case DlpWarnDialog::Restriction::kFiles:
      DCHECK(options.files_action.has_value());
      return GetDialogButtonOkLabelForFiles(options.files_action.value());
  }
}

// Returns the Cancel button label for |restriction|.
const std::u16string GetDialogButtonCancelLabel(
    DlpWarnDialog::Restriction restriction) {
  switch (restriction) {
    case DlpWarnDialog::Restriction::kVideoCapture:
      return l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_VIDEO_CAPTURE_WARN_CANCEL_BUTTON);
    case DlpWarnDialog::Restriction::kScreenCapture:
    case DlpWarnDialog::Restriction::kPrinting:
    case DlpWarnDialog::Restriction::kScreenShare:
    case DlpWarnDialog::Restriction::kFiles:
      return l10n_util::GetStringUTF16(IDS_POLICY_DLP_WARN_CANCEL_BUTTON);
  }
}

// Returns the title for |restriction|.
const std::u16string GetTitle(DlpWarnDialog::DlpWarnDialogOptions options) {
  switch (options.restriction) {
    case DlpWarnDialog::Restriction::kScreenCapture:
      return l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_SCREEN_CAPTURE_WARN_TITLE);
    case DlpWarnDialog::Restriction::kVideoCapture:
      return l10n_util::GetStringUTF16(IDS_POLICY_DLP_VIDEO_CAPTURE_WARN_TITLE);
    case DlpWarnDialog::Restriction::kPrinting:
      return l10n_util::GetStringUTF16(IDS_POLICY_DLP_PRINTING_WARN_TITLE);
    case DlpWarnDialog::Restriction::kScreenShare:
      return l10n_util::GetStringUTF16(IDS_POLICY_DLP_SCREEN_SHARE_WARN_TITLE);
    case DlpWarnDialog::Restriction::kFiles:
      DCHECK(options.files_action.has_value());
      return GetTitleForFiles(options.files_action.value(),
                              options.confidential_files.size());
  }
}

// Returns the message for |restriction|.
const std::u16string GetMessage(DlpWarnDialog::DlpWarnDialogOptions options) {
  switch (options.restriction) {
    case DlpWarnDialog::Restriction::kScreenCapture:
      return l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_SCREEN_CAPTURE_WARN_MESSAGE);
    case DlpWarnDialog::Restriction::kVideoCapture:
      return l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_VIDEO_CAPTURE_WARN_MESSAGE);
    case DlpWarnDialog::Restriction::kPrinting:
      return l10n_util::GetStringUTF16(IDS_POLICY_DLP_PRINTING_WARN_MESSAGE);
    case DlpWarnDialog::Restriction::kScreenShare:
      DCHECK(options.application_title.has_value());
      return l10n_util::GetStringFUTF16(
          IDS_POLICY_DLP_SCREEN_SHARE_WARN_MESSAGE,
          options.application_title.value());
    case DlpWarnDialog::Restriction::kFiles:
      DCHECK(options.files_action.has_value());
      return GetMessageForFiles(options);
  }
}

// Constructs and adds the top part of the dialog, containing the managed
// icon, dialog title and the informative text.
void AddGeneralInformation(views::View* upper_panel,
                           DlpWarnDialog::DlpWarnDialogOptions options) {
// TODO(crbug.com/1261496) Enable dynamic UI color & theme in lacros
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // When #dark-light-mode flag is disabled (default setting), the color mode is
  // by default set to dark mode. The warn dialog has white background for the
  // default setting, so it should use light mode color palette.
  ash::ScopedLightModeAsDefault scoped_light_mode_as_default;
  ash::ColorProvider* color_provider = ash::ColorProvider::Get();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  views::BoxLayout* layout =
      upper_panel->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical, kTopPanelInsets,
          kBetweenChildSpacing));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);

  views::ImageView* managed_icon =
      upper_panel->AddChildView(std::make_unique<views::ImageView>());
#if BUILDFLAG(IS_CHROMEOS_ASH)
  auto color = color_provider->GetContentLayerColor(
      ash::ColorProvider::ContentLayerType::kIconColorPrimary);
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  // TODO(crbug.com/1261496) Enable dynamic UI color & theme in lacros
  auto color = SK_ColorGRAY;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  managed_icon->SetImage(gfx::CreateVectorIcon(vector_icons::kBusinessIcon,
                                               kManagedIconSize, color));

  views::Label* title_label = upper_panel->AddChildView(
      std::make_unique<views::Label>(GetTitle(options)));
  title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_label->SetAllowCharacterBreak(true);
// TODO(crbug.com/1261496) Enable dynamic UI color & theme in lacros
#if BUILDFLAG(IS_CHROMEOS_ASH)
  title_label->SetEnabledColor(color_provider->GetContentLayerColor(
      ash::ColorProvider::ContentLayerType::kTextColorPrimary));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  title_label->SetFontList(gfx::FontList({kFontName}, gfx::Font::NORMAL,
                                         kTitleFontSize,
                                         gfx::Font::Weight::MEDIUM));
  title_label->SetLineHeight(kTitleLineHeight);

  views::Label* message = upper_panel->AddChildView(
      std::make_unique<views::Label>(GetMessage(options)));
  message->SetMultiLine(true);
  message->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  message->SetAllowCharacterBreak(true);
// TODO(crbug.com/1261496) Enable dynamic UI color & theme in lacros
#if BUILDFLAG(IS_CHROMEOS_ASH)
  message->SetEnabledColor(color_provider->GetContentLayerColor(
      ash::ColorProvider::ContentLayerType::kTextColorSecondary));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  message->SetFontList(gfx::FontList({kFontName}, gfx::Font::NORMAL,
                                     kBodyFontSize, gfx::Font::Weight::NORMAL));
  message->SetLineHeight(kBodyLineHeight);
}

// TODO(crbug.com/682266) Remove this function.
int GetMaxConfidentialTitleWidth() {
  int total_width = views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH);
  int margin_width = kMarginInsets.width() + kConfidentialListInsets.width() +
                     kConfidentialRowInsets.width();
  int image_width = kFaviconSize;
  int spacing = kBetweenChildSpacing;
  return total_width - margin_width - image_width - spacing;
}

// Adds |confidential_icon| and |confidential_title| to the container.
void AddConfidentialContentRow(views::View* container,
                               const gfx::ImageSkia& confidential_icon,
                               const std::u16string& confidential_title) {
// TODO(crbug.com/1261496) Enable dynamic UI color & theme in lacros
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // When #dark-light-mode flag is disabled (default setting), the color mode is
  // by default set to dark mode. The warn dialog has white background for the
  // default setting, so it should use light mode color palette.
  ash::ScopedLightModeAsDefault scoped_light_mode_as_default;
  ash::ColorProvider* color_provider = ash::ColorProvider::Get();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  views::View* row = container->AddChildView(std::make_unique<views::View>());
  row->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, kConfidentialRowInsets,
      kBetweenChildSpacing));

  views::ImageView* icon =
      row->AddChildView(std::make_unique<views::ImageView>());
  icon->SetImageSize(gfx::Size(kFaviconSize, kFaviconSize));
  icon->SetImage(confidential_icon);

  views::Label* title =
      row->AddChildView(std::make_unique<views::Label>(confidential_title));
  title->SetMultiLine(true);
  // TODO(crbug.com/682266) Remove the next line that sets the line size.
  title->SetMaximumWidth(GetMaxConfidentialTitleWidth());
  title->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title->SetAllowCharacterBreak(true);
// TODO(crbug.com/1261496) Enable dynamic UI color & theme in lacros
#if BUILDFLAG(IS_CHROMEOS_ASH)
  title->SetEnabledColor(color_provider->GetContentLayerColor(
      ash::ColorProvider::ContentLayerType::kTextColorSecondary));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  title->SetFontList(gfx::FontList({kFontName}, gfx::Font::NORMAL,
                                   kBodyFontSize, gfx::Font::Weight::NORMAL));
  title->SetLineHeight(kConfidentialContentLineHeight);
}

// Adds a scrollable child view to |parent| that lists the information from
// |confidential_files| if |restriction| is kFiles, otherwise from
// |confidential_contents|. No-op if no contents or files are given.
void MaybeAddConfidentialRows(
    views::View* parent,
    DlpWarnDialog::Restriction restriction,
    const DlpConfidentialContents& confidential_contents,
    const std::vector<DlpConfidentialFile>& confidential_files) {
  if (restriction == DlpWarnDialog::Restriction::kFiles &&
      confidential_files.empty()) {
    return;
  }
  if (restriction != DlpWarnDialog::Restriction::kFiles &&
      confidential_contents.IsEmpty()) {
    return;
  }

  views::ScrollView* scroll_view =
      parent->AddChildView(std::make_unique<views::ScrollView>());
  scroll_view->ClipHeightTo(0, kConfidentialContentListMaxHeight);
  views::View* container =
      scroll_view->SetContents(std::make_unique<views::View>());
  views::BoxLayout* layout =
      container->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical, kConfidentialListInsets,
          /*between_child_spacing=*/0));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);

  if (restriction == DlpWarnDialog::Restriction::kFiles) {
    for (const DlpConfidentialFile& confidential_file : confidential_files) {
      AddConfidentialContentRow(container, confidential_file.icon,
                                confidential_file.title);
    }
  } else {
    for (const DlpConfidentialContent& confidential_content :
         confidential_contents.GetContents()) {
      AddConfidentialContentRow(container, confidential_content.icon,
                                confidential_content.title);
    }
  }
}

}  // namespace

DlpWarnDialog::DlpWarnDialogOptions::DlpWarnDialogOptions(
    Restriction restriction)
    : restriction(restriction) {}

DlpWarnDialog::DlpWarnDialogOptions::DlpWarnDialogOptions(
    Restriction restriction,
    DlpConfidentialContents confidential_contents)
    : restriction(restriction), confidential_contents(confidential_contents) {}

DlpWarnDialog::DlpWarnDialogOptions::DlpWarnDialogOptions(
    Restriction restriction,
    DlpConfidentialContents confidential_contents,
    const std::u16string& application_title_)
    : restriction(restriction), confidential_contents(confidential_contents) {
  application_title.emplace(application_title_);
}

DlpWarnDialog::DlpWarnDialogOptions::DlpWarnDialogOptions(
    Restriction restriction,
    const std::vector<DlpConfidentialFile>& confidential_files,
    absl::optional<DlpRulesManager::Component> dst_component,
    const absl::optional<std::string>& destination_pattern,
    DlpFilesController::FileAction files_action)
    : restriction(restriction),
      confidential_files(confidential_files),
      destination_component(dst_component),
      destination_pattern(destination_pattern),
      files_action(files_action) {
  DCHECK(restriction == Restriction::kFiles);
}

DlpWarnDialog::DlpWarnDialogOptions::DlpWarnDialogOptions(
    const DlpWarnDialogOptions& other) = default;

DlpWarnDialog::DlpWarnDialogOptions&
DlpWarnDialog::DlpWarnDialogOptions::operator=(
    const DlpWarnDialogOptions& other) = default;

DlpWarnDialog::DlpWarnDialogOptions::~DlpWarnDialogOptions() = default;

DlpWarnDialog::DlpWarnDialog(OnDlpRestrictionCheckedCallback callback,
                             DlpWarnDialogOptions options) {
  auto split = base::SplitOnceCallback(std::move(callback));
  SetAcceptCallback(base::BindOnce(std::move(split.first), true));
  SetCancelCallback(base::BindOnce(std::move(split.second), false));

  SetModalType(ui::MODAL_TYPE_SYSTEM);

  SetShowCloseButton(false);
  SetButtonLabel(ui::DIALOG_BUTTON_OK, GetDialogButtonOkLabel(options));
  SetButtonLabel(ui::DIALOG_BUTTON_CANCEL,
                 GetDialogButtonCancelLabel(options.restriction));

  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));
  set_corner_radius(kDialogCornerRadius);
  set_margins(kMarginInsets);

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  AddGeneralInformation(AddChildView(std::make_unique<views::View>()), options);
  MaybeAddConfidentialRows(this, options.restriction,
                           options.confidential_contents,
                           options.confidential_files);
}

BEGIN_METADATA(DlpWarnDialog, views::DialogDelegateView)
END_METADATA

}  // namespace policy
