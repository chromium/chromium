// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/scanning/scanning_handler.h"

#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/webui/scanning/scanning_app_delegate.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/shell_dialogs/select_file_policy.h"
#include "ui/shell_dialogs/selected_file_info.h"

namespace {

constexpr char kBaseName[] = "baseName";
constexpr char kFilePath[] = "filePath";

}  // namespace

namespace ash {

ScanningHandler::ScanningHandler(
    std::unique_ptr<ScanningAppDelegate> scanning_app_delegate)
    : scanning_app_delegate_(std::move(scanning_app_delegate)),
      task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {}

ScanningHandler::~ScanningHandler() {
  if (select_file_dialog_)
    select_file_dialog_->ListenerDestroyed();
}

void ScanningHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "initialize", base::BindRepeating(&ScanningHandler::HandleInitialize,
                                        base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "requestScanToLocation",
      base::BindRepeating(&ScanningHandler::HandleRequestScanToLocation,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "showFileInLocation",
      base::BindRepeating(&ScanningHandler::HandleShowFileInLocation,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "getPluralString",
      base::BindRepeating(&ScanningHandler::HandleGetPluralString,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "getMyFilesPath",
      base::BindRepeating(&ScanningHandler::HandleGetMyFilesPath,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "openFilesInMediaApp",
      base::BindRepeating(&ScanningHandler::HandleOpenFilesInMediaApp,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "saveScanSettings",
      base::BindRepeating(&ScanningHandler::HandleSaveScanSettings,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "getScanSettings",
      base::BindRepeating(&ScanningHandler::HandleGetScanSettings,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "ensureValidFilePath",
      base::BindRepeating(&ScanningHandler::HandleEnsureValidFilePath,
                          base::Unretained(this)));
}

void ScanningHandler::FileSelected(const ui::SelectedFileInfo& file,
                                   int index) {
  DCHECK(IsJavascriptAllowed());

  select_file_dialog_ = nullptr;
  ResolveJavascriptCallback(base::Value(scan_location_callback_id_),
                            CreateSelectedPathValue(file.path()));
}

void ScanningHandler::FileSelectionCanceled() {
  DCHECK(IsJavascriptAllowed());

  select_file_dialog_ = nullptr;
  ResolveJavascriptCallback(base::Value(scan_location_callback_id_),
                            CreateSelectedPathValue(base::FilePath()));
}

base::Value::Dict ScanningHandler::CreateSelectedPathValue(
    const base::FilePath& path) {
  base::Value::Dict selected_path;
  selected_path.Set(kFilePath, path.value());
  selected_path.Set(kBaseName,
                    scanning_app_delegate_->GetBaseNameFromPath(path));
  return selected_path;
}

void ScanningHandler::AddStringToPluralMap(const std::string& name,
                                           int string_id) {
  string_id_map_[name] = string_id;
}

void ScanningHandler::SetWebUIForTest(content::WebUI* web_ui) {
  set_web_ui(web_ui);
}

void ScanningHandler::HandleInitialize(const base::Value::List& args) {
  DCHECK(args.empty());
  AllowJavascript();
}

void ScanningHandler::HandleOpenFilesInMediaApp(const base::Value::List& args) {
  if (!IsJavascriptAllowed())
    return;

  CHECK_EQ(1U, args.size());
  const base::Value::List& value_list = args[0].GetList();
  DCHECK(!value_list.empty());

  std::vector<base::FilePath> file_paths;
  file_paths.reserve(value_list.size());
  for (const base::Value& file_path_value : value_list)
    file_paths.push_back(base::FilePath(file_path_value.GetString()));

  scanning_app_delegate_->OpenFilesInMediaApp(file_paths);
}

void ScanningHandler::HandleRequestScanToLocation(
    const base::Value::List& args) {
  if (!IsJavascriptAllowed())
    return;

  // Early return if the select file dialog is already active.
  if (select_file_dialog_)
    return;

  CHECK_EQ(1U, args.size());
  scan_location_callback_id_ = args[0].GetString();

  content::WebContents* web_contents = web_ui()->GetWebContents();
  gfx::NativeWindow owning_window =
      web_contents ? web_contents->GetTopLevelNativeWindow()
                   : gfx::NativeWindow();
  select_file_dialog_ = ui::SelectFileDialog::Create(
      this, scanning_app_delegate_->CreateChromeSelectFilePolicy());
  select_file_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_FOLDER,
      l10n_util::GetStringUTF16(IDS_SCANNING_APP_SELECT_DIALOG_TITLE),
      base::FilePath() /* default_path */, nullptr /* file_types */,
      0 /* file_type_index */,
      base::FilePath::StringType() /* default_extension */, owning_window);
}

void ScanningHandler::HandleShowFileInLocation(const base::Value::List& args) {
  if (!IsJavascriptAllowed())
    return;

  CHECK_EQ(2U, args.size());
  const std::string callback = args[0].GetString();
  const base::FilePath file_location(args[1].GetString());
  scanning_app_delegate_->ShowFileInFilesApp(
      file_location,
      base::BindOnce(&ScanningHandler::OnShowFileInLocation,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ScanningHandler::OnShowFileInLocation(const std::string& callback,
                                           const bool files_app_opened) {
  ResolveJavascriptCallback(base::Value(callback),
                            base::Value(files_app_opened));
}

void ScanningHandler::HandleGetPluralString(const base::Value::List& args) {
  if (!IsJavascriptAllowed())
    return;

  CHECK_EQ(3U, args.size());
  const std::string callback = args[0].GetString();
  const std::string name = args[1].GetString();
  const int count = args[2].GetInt();

  auto iter = string_id_map_.find(name);
  if (iter == string_id_map_.end()) {
    // Only reachable if the WebUI renderer is misbehaving.
    return;
  }

  const std::u16string localized_string =
      l10n_util::GetPluralStringFUTF16(iter->second, count);
  ResolveJavascriptCallback(base::Value(callback),
                            base::Value(localized_string));
}

void ScanningHandler::HandleGetMyFilesPath(const base::Value::List& args) {
  if (!IsJavascriptAllowed())
    return;

  CHECK_EQ(1U, args.size());
  const std::string& callback = args[0].GetString();

  const base::FilePath my_files_path = scanning_app_delegate_->GetMyFilesPath();
  ResolveJavascriptCallback(base::Value(callback),
                            base::Value(my_files_path.value()));
}

void ScanningHandler::HandleSaveScanSettings(const base::Value::List& args) {
  if (!IsJavascriptAllowed())
    return;

  CHECK_EQ(1U, args.size());
  const std::string& scan_settings = args[0].GetString();
  scanning_app_delegate_->SaveScanSettingsToPrefs(scan_settings);
}

void ScanningHandler::HandleGetScanSettings(const base::Value::List& args) {
  if (!IsJavascriptAllowed())
    return;

  CHECK_EQ(1U, args.size());
  const std::string& callback = args[0].GetString();

  ResolveJavascriptCallback(
      base::Value(callback),
      base::Value(scanning_app_delegate_->GetScanSettingsFromPrefs()));
}

void ScanningHandler::HandleEnsureValidFilePath(const base::Value::List& args) {
  if (!IsJavascriptAllowed())
    return;

  CHECK_EQ(2U, args.size());
  const std::string callback = args[0].GetString();
  const base::FilePath file_path(args[1].GetString());

  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&base::PathExists, file_path),
      base::BindOnce(&ScanningHandler::OnPathExists,
                     weak_ptr_factory_.GetWeakPtr(), file_path, callback));
}

void ScanningHandler::OnPathExists(const base::FilePath& file_path,
                                   const std::string& callback,
                                   bool file_path_exists) {
  // When |file_path| is not valid, return a dictionary with an empty file path.
  const bool file_path_valid =
      file_path_exists &&
      scanning_app_delegate_->IsFilePathSupported(file_path);
  ResolveJavascriptCallback(
      base::Value(callback),
      CreateSelectedPathValue(file_path_valid ? file_path : base::FilePath()));
}

}  // namespace ash
