// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_shortcut_win.h"

#include <windows.h>
#include <objbase.h>
#include <shlobj.h>
#include <propkey.h>
#include <wrl/client.h>

#include <string>

#include "base/files/file_path.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/scoped_propvariant.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace win {

void ValidatePathsAreEqual(const FilePath& expected_path,
                           const FilePath& actual_path) {
  wchar_t long_expected_path_chars[MAX_PATH] = {0};
  wchar_t long_actual_path_chars[MAX_PATH] = {0};

  // If |expected_path| is empty confirm immediately that |actual_path| is also
  // empty.
  if (expected_path.empty()) {
    EXPECT_TRUE(actual_path.empty());
    return;
  }

  // Proceed with LongPathName matching which will also confirm the paths exist.
  EXPECT_NE(0U, ::GetLongPathName(expected_path.value().c_str(),
                                  long_expected_path_chars, MAX_PATH))
      << "Failed to get LongPathName of " << expected_path.value();
  EXPECT_NE(0U, ::GetLongPathName(actual_path.value().c_str(),
                                  long_actual_path_chars, MAX_PATH))
      << "Failed to get LongPathName of " << actual_path.value();

  FilePath long_expected_path(long_expected_path_chars);
  FilePath long_actual_path(long_actual_path_chars);
  EXPECT_FALSE(long_expected_path.empty());
  EXPECT_FALSE(long_actual_path.empty());

  EXPECT_TRUE(base::FilePath::CompareEqualIgnoreCase(long_expected_path.value(),
                                                     long_actual_path.value()));
}

void ValidateShortcut(const FilePath& shortcut_path,
                      const ShortcutProperties& properties) {
  Microsoft::WRL::ComPtr<IShellLink> i_shell_link;
  Microsoft::WRL::ComPtr<IPersistFile> i_persist_file;

  wchar_t read_target[MAX_PATH] = {0};
  wchar_t read_working_dir[MAX_PATH] = {0};
  wchar_t read_arguments[MAX_PATH] = {0};
  wchar_t read_description[MAX_PATH] = {0};
  wchar_t read_icon[MAX_PATH] = {0};
  int read_icon_index = 0;

  HRESULT hr;

  // Initialize the shell interfaces.
  EXPECT_TRUE(SUCCEEDED(hr = ::CoCreateInstance(CLSID_ShellLink, NULL,
                                                CLSCTX_INPROC_SERVER,
                                                IID_PPV_ARGS(&i_shell_link))));
  if (FAILED(hr))
    return;

  EXPECT_TRUE(SUCCEEDED(hr = i_shell_link.As(&i_persist_file)));
  if (FAILED(hr))
    return;

  // Load the shortcut.
  EXPECT_TRUE(
      SUCCEEDED(hr = i_persist_file->Load(shortcut_path.value().c_str(), 0)))
      << "Failed to load shortcut at " << shortcut_path.value();
  if (FAILED(hr))
    return;

  if (properties.options & ShortcutProperties::PROPERTIES_TARGET) {
    EXPECT_TRUE(SUCCEEDED(
        i_shell_link->GetPath(read_target, MAX_PATH, NULL, SLGP_SHORTPATH)));
    ValidatePathsAreEqual(properties.target, FilePath(read_target));
  }

  if (properties.options & ShortcutProperties::PROPERTIES_WORKING_DIR) {
    EXPECT_TRUE(SUCCEEDED(
        i_shell_link->GetWorkingDirectory(read_working_dir, MAX_PATH)));
    ValidatePathsAreEqual(properties.working_dir, FilePath(read_working_dir));
  }

  if (properties.options & ShortcutProperties::PROPERTIES_ARGUMENTS) {
    EXPECT_TRUE(
        SUCCEEDED(i_shell_link->GetArguments(read_arguments, MAX_PATH)));
    EXPECT_EQ(properties.arguments, read_arguments);
  }

  if (properties.options & ShortcutProperties::PROPERTIES_DESCRIPTION) {
    EXPECT_TRUE(
        SUCCEEDED(i_shell_link->GetDescription(read_description, MAX_PATH)));
    EXPECT_EQ(properties.description, read_description);
  }

  if (properties.options & ShortcutProperties::PROPERTIES_ICON) {
    EXPECT_TRUE(SUCCEEDED(
        i_shell_link->GetIconLocation(read_icon, MAX_PATH, &read_icon_index)));
    ValidatePathsAreEqual(properties.icon, FilePath(read_icon));
    EXPECT_EQ(properties.icon_index, read_icon_index);
  }

  Microsoft::WRL::ComPtr<IPropertyStore> property_store;
  EXPECT_TRUE(SUCCEEDED(hr = i_shell_link.As(&property_store)));
  if (FAILED(hr))
    return;

  if (properties.options & ShortcutProperties::PROPERTIES_APP_ID) {
    ScopedPropVariant pv_app_id;
    EXPECT_EQ(S_OK, property_store->GetValue(PKEY_AppUserModel_ID,
                                             pv_app_id.Receive()));
    switch (pv_app_id.get().vt) {
      case VT_EMPTY:
        EXPECT_TRUE(properties.app_id.empty());
        break;
      case VT_LPWSTR:
        EXPECT_EQ(properties.app_id, pv_app_id.get().pwszVal);
        break;
      default:
        ADD_FAILURE() << "Unexpected variant type: " << pv_app_id.get().vt;
    }
  }

  if (properties.options & ShortcutProperties::PROPERTIES_DUAL_MODE) {
    ScopedPropVariant pv_dual_mode;
    EXPECT_EQ(S_OK, property_store->GetValue(PKEY_AppUserModel_IsDualMode,
                                             pv_dual_mode.Receive()));
    switch (pv_dual_mode.get().vt) {
      case VT_EMPTY:
        EXPECT_FALSE(properties.dual_mode);
        break;
      case VT_BOOL:
        EXPECT_EQ(properties.dual_mode,
                  static_cast<bool>(pv_dual_mode.get().boolVal));
        break;
      default:
        ADD_FAILURE() << "Unexpected variant type: " << pv_dual_mode.get().vt;
    }
  }
}

}  // namespace win
}  // namespace base
