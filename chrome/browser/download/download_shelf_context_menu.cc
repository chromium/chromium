// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_shelf_context_menu.h"

#include <memory>

#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_commands.h"
#include "chrome/browser/download/download_stats.h"
#include "chrome/grit/generated_resources.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/common/content_features.h"
#include "extensions/common/extension.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/gfx/color_palette.h"

using InsecureDownloadStatus = download::DownloadItem::InsecureDownloadStatus;

bool DownloadShelfContextMenu::WantsContextMenu(
    DownloadUIModel* download_model) {
  return !download_model->IsDangerous() || download_model->MightBeMalicious() ||
         download_model->IsInsecure();
}

DownloadShelfContextMenu::~DownloadShelfContextMenu() {
  DetachFromDownloadItem();
}

DownloadShelfContextMenu::DownloadShelfContextMenu(
    base::WeakPtr<DownloadUIModel> download)
    : download_(download), download_commands_(new DownloadCommands(download)) {
  DCHECK(download_);
}

void DownloadShelfContextMenu::RecordCommandsEnabled(
    ui::SimpleMenuModel* model) {
  // Meant to be kept up-to-date with DownloadCommands::Command

  if (download_commands_enabled_recorded_) {
    return;
  }

  for (int command_int = 0; command_int <= DownloadCommands::Command::kMaxValue;
       command_int++) {
    if (model->GetIndexOfCommandId(command_int).has_value() &&
        IsCommandIdEnabled(command_int)) {
      DownloadCommands::Command download_command =
          static_cast<DownloadCommands::Command>(command_int);
      base::UmaHistogramEnumeration(
          "Download.ShelfContextMenuAction",
          DownloadCommandToShelfAction(download_command,
                                       /*clicked=*/false));
    }
  }

  download_commands_enabled_recorded_ = true;
}

ui::SimpleMenuModel* DownloadShelfContextMenu::GetMenuModel() {
  ui::SimpleMenuModel* model = nullptr;

  if (!download_)
    return nullptr;

  DCHECK(WantsContextMenu(download_.get()));

  bool is_download = download_->GetDownloadItem() != nullptr;

  if (download_->IsInsecure()) {
    model = GetInsecureDownloadMenuModel();
  } else if (ChromeDownloadManagerDelegate::IsDangerTypeBlocked(
                 download_->GetDangerType())) {
    model = GetInterruptedMenuModel(is_download);
  } else if (download_->GetDangerType() ==
             download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_SCANNING) {
    model = GetDeepScanningMenuModel(is_download);
  } else if (download_->IsMalicious()) {
    model = GetMaliciousMenuModel(is_download);
  } else if (download_->MightBeMalicious()) {
    model = GetMaybeMaliciousMenuModel(is_download);
  } else if (download_->GetState() == download::DownloadItem::COMPLETE) {
    model = GetFinishedMenuModel(is_download);
  } else if (download_->GetState() == download::DownloadItem::INTERRUPTED) {
    model = GetInterruptedMenuModel(is_download);
  } else if (download_->IsPaused()) {
    model = GetInProgressPausedMenuModel(is_download);
  } else {
    model = GetInProgressMenuModel(is_download);
  }
  RecordCommandsEnabled(model);
  return model;
}

bool DownloadShelfContextMenu::IsCommandIdEnabled(int command_id) const {
  if (!download_commands_)
    return false;

  return download_commands_->IsCommandEnabled(
      static_cast<DownloadCommands::Command>(command_id));
}

bool DownloadShelfContextMenu::IsCommandIdChecked(int command_id) const {
  if (!download_commands_)
    return false;

  return download_commands_->IsCommandChecked(
      static_cast<DownloadCommands::Command>(command_id));
}

bool DownloadShelfContextMenu::IsCommandIdVisible(int command_id) const {
  if (!download_commands_)
    return false;

  return download_commands_->IsCommandVisible(
      static_cast<DownloadCommands::Command>(command_id));
}

void DownloadShelfContextMenu::ExecuteCommand(int command_id, int event_flags) {
  if (!download_commands_)
    return;

  download_commands_->ExecuteCommand(
      static_cast<DownloadCommands::Command>(command_id));
}

bool DownloadShelfContextMenu::IsItemForCommandIdDynamic(int command_id) const {
  return false;
}

std::u16string DownloadShelfContextMenu::GetLabelForCommandId(
    int command_id) const {
  int id = -1;

  switch (static_cast<DownloadCommands::Command>(command_id)) {
    case DownloadCommands::OPEN_WHEN_COMPLETE:
      if (download_ && !download_->IsDone())
        id = IDS_DOWNLOAD_MENU_OPEN_WHEN_COMPLETE;
      else
        id = IDS_DOWNLOAD_MENU_OPEN;
      break;
    case DownloadCommands::PAUSE:
      id = IDS_DOWNLOAD_MENU_PAUSE_ITEM;
      break;
    case DownloadCommands::RESUME:
      id = IDS_DOWNLOAD_MENU_RESUME_ITEM;
      break;
    case DownloadCommands::SHOW_IN_FOLDER:
      id = IDS_DOWNLOAD_MENU_SHOW;
      break;
    case DownloadCommands::DISCARD:
      id = IDS_DOWNLOAD_MENU_DISCARD;
      break;
    case DownloadCommands::KEEP:
      id = IDS_DOWNLOAD_MENU_KEEP;
      break;
    case DownloadCommands::ALWAYS_OPEN_TYPE: {
      if (download_commands_) {
        bool can_open_pdf_in_system_viewer =
            download_commands_->CanOpenPdfInSystemViewer();
        if (can_open_pdf_in_system_viewer) {
          id = IDS_DOWNLOAD_MENU_PLATFORM_OPEN_ALWAYS;
          break;
        }
      }
      id = IDS_DOWNLOAD_MENU_ALWAYS_OPEN_TYPE;
      break;
    }
    case DownloadCommands::PLATFORM_OPEN:
      id = IDS_DOWNLOAD_MENU_PLATFORM_OPEN;
      break;
    case DownloadCommands::CANCEL:
      id = IDS_DOWNLOAD_MENU_CANCEL;
      break;
    case DownloadCommands::LEARN_MORE_SCANNING:
      id = IDS_DOWNLOAD_MENU_LEARN_MORE_SCANNING;
      break;
    case DownloadCommands::LEARN_MORE_INTERRUPTED:
      id = IDS_DOWNLOAD_MENU_LEARN_MORE_INTERRUPTED;
      break;
    case DownloadCommands::LEARN_MORE_INSECURE_DOWNLOAD:
      id = IDS_DOWNLOAD_MENU_LEARN_MORE_INSECURE;
      break;
    case DownloadCommands::COPY_TO_CLIPBOARD:
      // This command is implemented only for the Download notification.
      NOTREACHED_IN_MIGRATION();
      break;
    case DownloadCommands::DEEP_SCAN:
      id = IDS_DOWNLOAD_MENU_DEEP_SCAN;
      break;
    case DownloadCommands::BYPASS_DEEP_SCANNING_AND_OPEN:
      id = IDS_OPEN_DOWNLOAD_NOW;
      break;
    // These commands are not supported on the context menu.
    case DownloadCommands::REVIEW:
    case DownloadCommands::RETRY:
    case DownloadCommands::CANCEL_DEEP_SCAN:
    case DownloadCommands::LEARN_MORE_DOWNLOAD_BLOCKED:
    case DownloadCommands::OPEN_SAFE_BROWSING_SETTING:
    case DownloadCommands::BYPASS_DEEP_SCANNING:
    case DownloadCommands::OPEN_WITH_MEDIA_APP:
    case DownloadCommands::EDIT_WITH_MEDIA_APP:
      NOTREACHED_IN_MIGRATION();
      break;
  }
  CHECK(id != -1);
  return l10n_util::GetStringUTF16(id);
}

void DownloadShelfContextMenu::DetachFromDownloadItem() {
  if (!download_)
    return;

  download_commands_.reset();
  download_ = nullptr;
}

void DownloadShelfContextMenu::OnDownloadDestroyed() {
  DetachFromDownloadItem();
}

ui::SimpleMenuModel* DownloadShelfContextMenu::GetInProgressMenuModel(
    bool is_download) {
  if (in_progress_download_menu_model_)
    return in_progress_download_menu_model_.get();

  in_progress_download_menu_model_ =
      std::make_unique<ui::SimpleMenuModel>(this);

  if (is_download) {
    in_progress_download_menu_model_->AddCheckItem(
        DownloadCommands::OPEN_WHEN_COMPLETE,
        GetLabelForCommandId(DownloadCommands::OPEN_WHEN_COMPLETE));
    AddAutoOpenToMenu(in_progress_download_menu_model_.get());
    in_progress_download_menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
  }

  in_progress_download_menu_model_->AddItem(
      DownloadCommands::PAUSE, GetLabelForCommandId(DownloadCommands::PAUSE));

  if (is_download) {
    in_progress_download_menu_model_->AddItem(
        DownloadCommands::SHOW_IN_FOLDER,
        GetLabelForCommandId(DownloadCommands::SHOW_IN_FOLDER));
  }

  in_progress_download_menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
  in_progress_download_menu_model_->AddItem(
      DownloadCommands::CANCEL, GetLabelForCommandId(DownloadCommands::CANCEL));

  return in_progress_download_menu_model_.get();
}

ui::SimpleMenuModel* DownloadShelfContextMenu::GetInProgressPausedMenuModel(
    bool is_download) {
  if (in_progress_download_paused_menu_model_)
    return in_progress_download_paused_menu_model_.get();

  in_progress_download_paused_menu_model_ =
      std::make_unique<ui::SimpleMenuModel>(this);

  if (is_download) {
    in_progress_download_paused_menu_model_->AddCheckItem(
        DownloadCommands::OPEN_WHEN_COMPLETE,
        GetLabelForCommandId(DownloadCommands::OPEN_WHEN_COMPLETE));
    AddAutoOpenToMenu(in_progress_download_paused_menu_model_.get());
    in_progress_download_paused_menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
  }

  in_progress_download_paused_menu_model_->AddItem(
      DownloadCommands::RESUME, GetLabelForCommandId(DownloadCommands::RESUME));

  if (is_download) {
    in_progress_download_paused_menu_model_->AddItem(
        DownloadCommands::SHOW_IN_FOLDER,
        GetLabelForCommandId(DownloadCommands::SHOW_IN_FOLDER));
  }

  in_progress_download_paused_menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
  in_progress_download_paused_menu_model_->AddItem(
      DownloadCommands::CANCEL, GetLabelForCommandId(DownloadCommands::CANCEL));

  return in_progress_download_paused_menu_model_.get();
}

ui::SimpleMenuModel* DownloadShelfContextMenu::GetFinishedMenuModel(
    bool is_download) {
  if (finished_download_menu_model_)
    return finished_download_menu_model_.get();

  finished_download_menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);

  if (is_download) {
    finished_download_menu_model_->AddItem(
        DownloadCommands::OPEN_WHEN_COMPLETE,
        GetLabelForCommandId(DownloadCommands::OPEN_WHEN_COMPLETE));
  }

  finished_download_menu_model_->AddItem(
      DownloadCommands::PLATFORM_OPEN,
      GetLabelForCommandId(DownloadCommands::PLATFORM_OPEN));

  if (is_download) {
    AddAutoOpenToMenu(finished_download_menu_model_.get());
  }
  finished_download_menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);

  if (is_download) {
    finished_download_menu_model_->AddItem(
        DownloadCommands::SHOW_IN_FOLDER,
        GetLabelForCommandId(DownloadCommands::SHOW_IN_FOLDER));
    finished_download_menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
  }

  finished_download_menu_model_->AddItem(
      DownloadCommands::CANCEL, GetLabelForCommandId(DownloadCommands::CANCEL));

  return finished_download_menu_model_.get();
}

ui::SimpleMenuModel* DownloadShelfContextMenu::GetInterruptedMenuModel(
    bool is_download) {
  if (interrupted_download_menu_model_)
    return interrupted_download_menu_model_.get();

  interrupted_download_menu_model_ =
      std::make_unique<ui::SimpleMenuModel>(this);

  interrupted_download_menu_model_->AddItem(
      DownloadCommands::RESUME, GetLabelForCommandId(DownloadCommands::RESUME));
#if BUILDFLAG(IS_WIN)
  // The Help Center article is currently Windows specific.
  // TODO(asanka): Enable this for other platforms when the article is expanded
  // for other platforms.
  interrupted_download_menu_model_->AddItem(
      DownloadCommands::LEARN_MORE_INTERRUPTED,
      GetLabelForCommandId(DownloadCommands::LEARN_MORE_INTERRUPTED));
#endif
  interrupted_download_menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
  interrupted_download_menu_model_->AddItem(
      DownloadCommands::CANCEL, GetLabelForCommandId(DownloadCommands::CANCEL));

  return interrupted_download_menu_model_.get();
}

ui::SimpleMenuModel* DownloadShelfContextMenu::GetMaybeMaliciousMenuModel(
    bool is_download) {
  if (maybe_malicious_download_menu_model_)
    return maybe_malicious_download_menu_model_.get();

  maybe_malicious_download_menu_model_ =
      std::make_unique<ui::SimpleMenuModel>(this);

  maybe_malicious_download_menu_model_->AddItem(
      DownloadCommands::KEEP, GetLabelForCommandId(DownloadCommands::KEEP));
  maybe_malicious_download_menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
  maybe_malicious_download_menu_model_->AddItem(
      DownloadCommands::LEARN_MORE_SCANNING,
      GetLabelForCommandId(DownloadCommands::LEARN_MORE_SCANNING));
  return maybe_malicious_download_menu_model_.get();
}

ui::SimpleMenuModel* DownloadShelfContextMenu::GetMaliciousMenuModel(
    bool is_download) {
  if (malicious_download_menu_model_)
    return malicious_download_menu_model_.get();

  malicious_download_menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
  malicious_download_menu_model_->AddItem(
      DownloadCommands::LEARN_MORE_SCANNING,
      GetLabelForCommandId(DownloadCommands::LEARN_MORE_SCANNING));

  return malicious_download_menu_model_.get();
}

ui::SimpleMenuModel* DownloadShelfContextMenu::GetDeepScanningMenuModel(
    bool is_download) {
  if (deep_scanning_menu_model_)
    return deep_scanning_menu_model_.get();

  deep_scanning_menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
  deep_scanning_menu_model_->AddItem(
      DownloadCommands::DEEP_SCAN,
      GetLabelForCommandId(DownloadCommands::DEEP_SCAN));

  deep_scanning_menu_model_->AddItem(
      DownloadCommands::DISCARD,
      GetLabelForCommandId(DownloadCommands::DISCARD));

  deep_scanning_menu_model_->AddItem(
      DownloadCommands::BYPASS_DEEP_SCANNING_AND_OPEN,
      GetLabelForCommandId(DownloadCommands::BYPASS_DEEP_SCANNING_AND_OPEN));

  deep_scanning_menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);

  if (is_download) {
    deep_scanning_menu_model_->AddItem(
        DownloadCommands::SHOW_IN_FOLDER,
        GetLabelForCommandId(DownloadCommands::SHOW_IN_FOLDER));
    deep_scanning_menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
  }

  deep_scanning_menu_model_->AddItem(
      DownloadCommands::CANCEL, GetLabelForCommandId(DownloadCommands::CANCEL));

  return deep_scanning_menu_model_.get();
}

ui::SimpleMenuModel* DownloadShelfContextMenu::GetInsecureDownloadMenuModel() {
  if (insecure_download_menu_model_)
    return insecure_download_menu_model_.get();

  insecure_download_menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);

  if (download_->GetInsecureDownloadStatus() == InsecureDownloadStatus::WARN) {
    insecure_download_menu_model_->AddItem(
        DownloadCommands::DISCARD,
        GetLabelForCommandId(DownloadCommands::DISCARD));
  } else {
    insecure_download_menu_model_->AddItem(
        DownloadCommands::KEEP, GetLabelForCommandId(DownloadCommands::KEEP));
  }

  insecure_download_menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
  insecure_download_menu_model_->AddItem(
      DownloadCommands::LEARN_MORE_INSECURE_DOWNLOAD,
      GetLabelForCommandId(DownloadCommands::LEARN_MORE_INSECURE_DOWNLOAD));

  return insecure_download_menu_model_.get();
}

void DownloadShelfContextMenu::AddAutoOpenToMenu(ui::SimpleMenuModel* menu) {
  if (!download_)
    return;

  if (download_->IsOpenWhenCompleteByPolicy()) {
    menu->AddItemWithIcon(
        DownloadCommands::ALWAYS_OPEN_TYPE,
        GetLabelForCommandId(DownloadCommands::ALWAYS_OPEN_TYPE),
        ui::ImageModel::FromVectorIcon(vector_icons::kBusinessIcon,
                                       ui::kColorIcon,
                                       ui::SimpleMenuModel::kDefaultIconSize));
  } else {
    menu->AddCheckItem(
        DownloadCommands::ALWAYS_OPEN_TYPE,
        GetLabelForCommandId(DownloadCommands::ALWAYS_OPEN_TYPE));
  }
}
