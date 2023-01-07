// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#include "chrome/browser/ash/dbus/vm/vm_applications_service_provider.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/shell_dialogs/select_file_dialog.h"

namespace ash {

using VmApplicationsServiceProviderTest = testing::Test;

TEST_F(VmApplicationsServiceProviderTest, ParseSelectFileDialogFileTypes) {
  VmApplicationsServiceProvider test;
  ui::SelectFileDialog::FileTypeInfo file_types;
  int file_type_index = 0;

  // Complex.
  test.ParseSelectFileDialogFileTypes("e1,e2:d1|,e3:d2|*", &file_types,
                                      &file_type_index);

  std::vector<std::vector<std::string>> exts{{"e1", "e2"}, {"e3"}};
  std::vector<std::u16string> descs{u"d1", u"d2"};
  EXPECT_EQ(file_types.extensions, exts);
  EXPECT_EQ(file_types.extension_description_overrides, descs);
  EXPECT_EQ(file_type_index, 2);
  EXPECT_TRUE(file_types.include_all_files);

  // Simple.
  test.ParseSelectFileDialogFileTypes("e1,e2", &file_types, &file_type_index);
  exts = {{"e1", "e2"}};
  descs = {u""};
  EXPECT_EQ(file_types.extensions, exts);
  EXPECT_EQ(file_types.extension_description_overrides, descs);
  EXPECT_EQ(file_type_index, 0);
  EXPECT_FALSE(file_types.include_all_files);
}

}  // namespace ash
