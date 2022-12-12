// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_SHELF_CONTEXT_MENU_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_SHELF_CONTEXT_MENU_H_

#include <memory>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/download/download_commands.h"
#include "chrome/browser/download/download_ui_model.h"
#include "components/download/public/common/download_item.h"
#include "ui/base/models/simple_menu_model.h"

// This class is responsible for the download shelf context menu. Platform
// specific subclasses are responsible for creating and running the menu.
//
// The DownloadItem corresponding to the context menu is observed for removal or
// destruction.
class DownloadShelfContextMenu : public ui::SimpleMenuModel::Delegate {
 public:
  // Only show a context menu for a dangerous download if it is malicious.
  static bool WantsContextMenu(DownloadUIModel* download_model);

  DownloadShelfContextMenu(const DownloadShelfContextMenu&) = delete;
  DownloadShelfContextMenu& operator=(const DownloadShelfContextMenu&) = delete;

  ~DownloadShelfContextMenu() override;

  // Called when download is destroyed.
  void OnDownloadDestroyed();

 protected:
  explicit DownloadShelfContextMenu(base::WeakPtr<DownloadUIModel> download);

  // Returns the correct menu model depending on the state of the download item.
  // Returns nullptr if the download was destroyed.
  ui::SimpleMenuModel* GetMenuModel();

  // ui::SimpleMenuModel::Delegate:
  bool IsCommandIdEnabled(int command_id) const override;
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdVisible(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;
  bool IsItemForCommandIdDynamic(int command_id) const override;
  std::u16string GetLabelForCommandId(int command_id) const override;

  DownloadUIModel* GetDownload() { return download_.get(); }

 private:
  friend class DownloadShelfContextMenuTest;
  FRIEND_TEST_ALL_PREFIXES(DownloadShelfContextMenuTest,
                           InvalidDownloadWontCrashContextMenu);
  FRIEND_TEST_ALL_PREFIXES(DownloadShelfContextMenuTest, RecordCommandsEnabled);

  // Detaches self from |download_item_|. Called when the DownloadItem is
  // destroyed or when this object is being destroyed.
  void DetachFromDownloadItem();

  ui::SimpleMenuModel* GetInProgressMenuModel(bool is_download);
  ui::SimpleMenuModel* GetInProgressPausedMenuModel(bool is_download);
  ui::SimpleMenuModel* GetFinishedMenuModel(bool is_download);
  ui::SimpleMenuModel* GetInterruptedMenuModel(bool is_download);
  ui::SimpleMenuModel* GetMaybeMaliciousMenuModel(bool is_download);
  ui::SimpleMenuModel* GetMaliciousMenuModel(bool is_download);
  ui::SimpleMenuModel* GetDeepScanningMenuModel(bool is_download);
  ui::SimpleMenuModel* GetInsecureDownloadMenuModel();

  void AddAutoOpenToMenu(ui::SimpleMenuModel* model);

  void RecordCommandsEnabled(ui::SimpleMenuModel* model);

  // We show slightly different menus if the download is in progress vs. if the
  // download has finished.
  std::unique_ptr<ui::SimpleMenuModel> in_progress_download_menu_model_;
  std::unique_ptr<ui::SimpleMenuModel> in_progress_download_paused_menu_model_;
  std::unique_ptr<ui::SimpleMenuModel> finished_download_menu_model_;
  std::unique_ptr<ui::SimpleMenuModel> interrupted_download_menu_model_;
  std::unique_ptr<ui::SimpleMenuModel> maybe_malicious_download_menu_model_;
  std::unique_ptr<ui::SimpleMenuModel> malicious_download_menu_model_;
  std::unique_ptr<ui::SimpleMenuModel> deep_scanning_menu_model_;
  std::unique_ptr<ui::SimpleMenuModel> insecure_download_menu_model_;

  // Whether or not a histogram has been emitted recording which
  // Download commands were enabled
  bool download_commands_enabled_recorded_ = false;

  // Information source.
  // Use WeakPtr because the context menu may outlive |download_|.
  base::WeakPtr<DownloadUIModel> download_;
  std::unique_ptr<DownloadCommands> download_commands_;
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_SHELF_CONTEXT_MENU_H_
