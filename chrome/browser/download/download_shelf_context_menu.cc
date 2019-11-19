// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_shelf_context_menu.h"

#include "build/build_config.h"
#include "chrome/grit/generated_resources.h"
#include "components/download/public/common/download_danger_type.h"
#include "content/public/common/content_features.h"
#include "extensions/common/extension.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_WIN)
#include "chrome/browser/ui/pdf/adobe_reader_info_win.h"
#endif

bool DownloadShelfContextMenu::WantsContextMenu(
    DownloadUIModel* download_model) {
  return !download_model->IsDangerous() || download_model->MightBeMalicious();
}

DownloadShelfContextMenu::~DownloadShelfContextMenu() {
  DetachFromDownloadItem();
}

DownloadShelfContextMenu::DownloadShelfContextMenu(DownloadUIModel* download)
    : download_(download), download_commands_(new DownloadCommands(download_)) {
  DCHECK(download_);
  download_->AddObserver(this);
}

ui::SimpleMenuModel* DownloadShelfContextMenu::GetMenuModel() {
  ui::SimpleMenuModel* model = NULL;

  if (!download_)
    return NULL;

  DCHECK(WantsContextMenu(download_));

  bool is_download = download_->download() != nullptr;

  if (download_->GetDangerType() ==
          download::DOWNLOAD_DANGER_TYPE_BLOCKED_PASSWORD_PROTECTED ||
      download_->GetDangerType() ==
          download::DOWNLOAD_DANGER_TYPE_BLOCKED_TOO_LARGE ||
      download_->GetDangerType() ==
          download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_BLOCK) {
    model = GetInterruptedMenuModel(is_download);
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

base::string16 DownloadShelfContextMenu::GetLabelForCommandId(
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
#if defined(OS_WIN)
        if (can_open_pdf_in_system_viewer) {
          id = IsAdobeReaderDefaultPDFViewer()
                   ? IDS_DOWNLOAD_MENU_ALWAYS_OPEN_PDF_IN_READER
                   : IDS_DOWNLOAD_MENU_PLATFORM_OPEN_ALWAYS;
          break;
        }
#elif defined(OS_MACOSX) || defined(OS_LINUX)
        if (can_open_pdf_in_system_viewer) {
          id = IDS_DOWNLOAD_MENU_PLATFORM_OPEN_ALWAYS;
          break;
        }
#endif
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
    case DownloadCommands::COPY_TO_CLIPBOARD:
    case DownloadCommands::ANNOTATE:
      // These commands are implemented only for the Download notification.
      NOTREACHED();
      break;
  }
  CHECK(id != -1);
  return l10n_util::GetStringUTF16(id);
}

void DownloadShelfContextMenu::DetachFromDownloadItem() {
  if (!download_)
    return;

  download_commands_.reset();
  download_->RemoveObserver(this);
  download_ = NULL;
}

void DownloadShelfContextMenu::OnDownloadDestroyed() {
  DetachFromDownloadItem();
}

ui::SimpleMenuModel* DownloadShelfContextMenu::GetInProgressMenuModel(
    bool is_download) {
  if (in_progress_download_menu_model_)
    return in_progress_download_menu_model_.get();

  in_progress_download_menu_model_.reset(new ui::SimpleMenuModel(this));

  if (is_download) {
    in_progress_download_menu_model_->AddCheckItem(
        DownloadCommands::OPEN_WHEN_COMPLETE,
        GetLabelForCommandId(DownloadCommands::OPEN_WHEN_COMPLETE));
    in_progress_download_menu_model_->AddCheckItem(
        DownloadCommands::ALWAYS_OPEN_TYPE,
        GetLabelForCommandId(DownloadCommands::ALWAYS_OPEN_TYPE));
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

  in_progress_download_paused_menu_model_.reset(new ui::SimpleMenuModel(this));

  if (is_download) {
    in_progress_download_paused_menu_model_->AddCheckItem(
        DownloadCommands::OPEN_WHEN_COMPLETE,
        GetLabelForCommandId(DownloadCommands::OPEN_WHEN_COMPLETE));
    in_progress_download_paused_menu_model_->AddCheckItem(
        DownloadCommands::ALWAYS_OPEN_TYPE,
        GetLabelForCommandId(DownloadCommands::ALWAYS_OPEN_TYPE));
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

  finished_download_menu_model_.reset(new ui::SimpleMenuModel(this));

  if (is_download) {
    finished_download_menu_model_->AddItem(
        DownloadCommands::OPEN_WHEN_COMPLETE,
        GetLabelForCommandId(DownloadCommands::OPEN_WHEN_COMPLETE));
  }

  finished_download_menu_model_->AddItem(
      DownloadCommands::PLATFORM_OPEN,
      GetLabelForCommandId(DownloadCommands::PLATFORM_OPEN));

  if (is_download) {
    finished_download_menu_model_->AddCheckItem(
        DownloadCommands::ALWAYS_OPEN_TYPE,
        GetLabelForCommandId(DownloadCommands::ALWAYS_OPEN_TYPE));
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

  interrupted_download_menu_model_.reset(new ui::SimpleMenuModel(this));

  interrupted_download_menu_model_->AddItem(
      DownloadCommands::RESUME, GetLabelForCommandId(DownloadCommands::RESUME));
#if defined(OS_WIN)
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

  maybe_malicious_download_menu_model_.reset(new ui::SimpleMenuModel(this));

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

  malicious_download_menu_model_.reset(new ui::SimpleMenuModel(this));
  malicious_download_menu_model_->AddItem(
      DownloadCommands::LEARN_MORE_SCANNING,
      GetLabelForCommandId(DownloadCommands::LEARN_MORE_SCANNING));

  return malicious_download_menu_model_.get();
}
