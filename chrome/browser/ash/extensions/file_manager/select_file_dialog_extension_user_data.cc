// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/file_manager/select_file_dialog_extension_user_data.h"

#include <optional>

#include "base/check_is_test.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_file_destination.h"
#include "content/public/browser/web_contents.h"
#include "ui/shell_dialogs/select_file_dialog.h"

const char kSelectFileDialogExtensionUserDataKey[] =
    "SelectFileDialogExtensionUserDataKey";

static policy::DlpFileDestination* g_fake_dialog_caller = nullptr;

SelectFileDialogExtensionUserData::~SelectFileDialogExtensionUserData() =
    default;

// static
void SelectFileDialogExtensionUserData::SetDialogDataForWebContents(
    content::WebContents* web_contents,
    const std::string& routing_id,
    ui::SelectFileDialog::Type type,
    std::optional<policy::DlpFileDestination> dialog_caller) {
  DCHECK(web_contents);
  web_contents->SetUserData(
      kSelectFileDialogExtensionUserDataKey,
      base::WrapUnique(new SelectFileDialogExtensionUserData(
          routing_id, type, std::move(dialog_caller))));
}

// static
std::string SelectFileDialogExtensionUserData::GetRoutingIdForWebContents(
    content::WebContents* web_contents) {
  // There's a race condition. This can be called from a callback after the
  // webcontents has been deleted.
  if (!web_contents) {
    LOG(WARNING) << "WebContents already destroyed.";
    return "";
  }

  SelectFileDialogExtensionUserData* data =
      static_cast<SelectFileDialogExtensionUserData*>(
          web_contents->GetUserData(kSelectFileDialogExtensionUserDataKey));
  return data ? data->routing_id() : "";
}

// static
ui::SelectFileDialog::Type
SelectFileDialogExtensionUserData::GetDialogTypeForWebContents(
    content::WebContents* web_contents) {
  // There's a race condition. This can be called from a callback after the
  // webcontents has been deleted.
  if (!web_contents) {
    LOG(WARNING) << "WebContents already destroyed.";
    return ui::SelectFileDialog::Type::SELECT_NONE;
  }

  SelectFileDialogExtensionUserData* data =
      static_cast<SelectFileDialogExtensionUserData*>(
          web_contents->GetUserData(kSelectFileDialogExtensionUserDataKey));
  return data ? data->type() : ui::SelectFileDialog::Type::SELECT_NONE;
}

// static
std::optional<policy::DlpFileDestination>
SelectFileDialogExtensionUserData::GetDialogCallerForWebContents(
    content::WebContents* web_contents) {
  // There's a race condition. This can be called from a callback after the
  // webcontents has been deleted.
  if (!web_contents) {
    LOG(WARNING) << "WebContents already destroyed.";
    return std::nullopt;
  }

  if (g_fake_dialog_caller) {
    CHECK_IS_TEST();
    return *g_fake_dialog_caller;
  }

  SelectFileDialogExtensionUserData* data =
      static_cast<SelectFileDialogExtensionUserData*>(
          web_contents->GetUserData(kSelectFileDialogExtensionUserDataKey));
  return data ? data->dialog_caller() : std::nullopt;
}

// static
void SelectFileDialogExtensionUserData::SetDialogCallerForTesting(
    policy::DlpFileDestination* dialog_caller) {
  g_fake_dialog_caller = dialog_caller;
}

SelectFileDialogExtensionUserData::SelectFileDialogExtensionUserData(
    const std::string& routing_id,
    ui::SelectFileDialog::Type type,
    std::optional<policy::DlpFileDestination> dialog_caller)
    : routing_id_(routing_id),
      type_(type),
      dialog_caller_(std::move(dialog_caller)) {}
