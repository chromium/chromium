// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/url_util.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "net/base/escape.h"

namespace file_manager {
namespace util {
namespace {

const char kAllowedPaths[] = "allowedPaths";
const char kNativePath[] = "nativePath";
const char kAnyPath[] = "anyPath";
const char kAnyPathOrUrl[] = "anyPathOrUrl";

// Returns a file manager URL for the given |path|.
GURL GetFileManagerUrl(const char* path) {
  return GURL(std::string("chrome-extension://") + kFileManagerAppId + path);
}

// Converts a numeric dialog type to a string.
std::string GetDialogTypeAsString(
    ui::SelectFileDialog::Type dialog_type) {
  std::string type_str;
  switch (dialog_type) {
    case ui::SelectFileDialog::SELECT_NONE:
      type_str = "full-page";
      break;

    case ui::SelectFileDialog::SELECT_FOLDER:
    case ui::SelectFileDialog::SELECT_EXISTING_FOLDER:
      type_str = "folder";
      break;

    case ui::SelectFileDialog::SELECT_UPLOAD_FOLDER:
      type_str = "upload-folder";
      break;

    case ui::SelectFileDialog::SELECT_SAVEAS_FILE:
      type_str = "saveas-file";
      break;

    case ui::SelectFileDialog::SELECT_OPEN_FILE:
      type_str = "open-file";
      break;

    case ui::SelectFileDialog::SELECT_OPEN_MULTI_FILE:
      type_str = "open-multi-file";
      break;
  }

  return type_str;
}

}  // namespace

GURL GetFileManagerMainPageUrl() {
  return GetFileManagerUrl("/main_modules.html");
}

GURL GetFileManagerMainPageUrlWithParams(
    ui::SelectFileDialog::Type type,
    const std::u16string& title,
    const GURL& current_directory_url,
    const GURL& selection_url,
    const std::string& target_name,
    const ui::SelectFileDialog::FileTypeInfo* file_types,
    int file_type_index,
    const std::string& search_query,
    bool show_android_picker_apps) {
  base::DictionaryValue arg_value;
  arg_value.SetString("type", GetDialogTypeAsString(type));
  arg_value.SetString("title", title);
  arg_value.SetString("currentDirectoryURL", current_directory_url.spec());
  arg_value.SetString("selectionURL", selection_url.spec());
  arg_value.SetString("targetName", target_name);
  arg_value.SetString("searchQuery", search_query);
  arg_value.SetBoolean("showAndroidPickerApps", show_android_picker_apps);

  if (file_types) {
    auto types_list = std::make_unique<base::ListValue>();
    for (size_t i = 0; i < file_types->extensions.size(); ++i) {
      auto extensions_list = std::make_unique<base::ListValue>();
      for (size_t j = 0; j < file_types->extensions[i].size(); ++j) {
        extensions_list->AppendString(file_types->extensions[i][j]);
      }

      auto dict = std::make_unique<base::DictionaryValue>();
      dict->Set("extensions", std::move(extensions_list));

      if (i < file_types->extension_description_overrides.size()) {
        std::u16string desc = file_types->extension_description_overrides[i];
        dict->SetString("description", desc);
      }

      // file_type_index is 1-based. 0 means no selection at all.
      dict->SetBoolean("selected",
                       (static_cast<size_t>(file_type_index) == (i + 1)));

      types_list->Set(i, std::move(dict));
    }
    arg_value.Set("typeList", std::move(types_list));

    arg_value.SetBoolean("includeAllFiles", file_types->include_all_files);
  }

  if (file_types) {
    switch (file_types->allowed_paths) {
      case ui::SelectFileDialog::FileTypeInfo::NATIVE_PATH:
        arg_value.SetString(kAllowedPaths, kNativePath);
        break;
      case ui::SelectFileDialog::FileTypeInfo::ANY_PATH:
        arg_value.SetString(kAllowedPaths, kAnyPath);
        break;
      case ui::SelectFileDialog::FileTypeInfo::ANY_PATH_OR_URL:
        arg_value.SetString(kAllowedPaths, kAnyPathOrUrl);
        break;
    }
  } else {
    arg_value.SetString(kAllowedPaths, kNativePath);
  }

  std::string json_args;
  base::JSONWriter::Write(arg_value, &json_args);

  std::string url = GetFileManagerMainPageUrl().spec() + '?' +
      net::EscapeUrlEncodedData(json_args,
                                false);  // Space to %20 instead of +.
  return GURL(url);
}

}  // namespace util
}  // namespace file_manager
