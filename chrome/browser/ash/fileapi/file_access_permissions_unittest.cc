// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/fileapi/file_access_permissions.h"

#include "extensions/common/extension.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace ash {

TEST(FileAccessPermissionsTest, FileAccessChecks) {
  base::FilePath good_dir(FILE_PATH_LITERAL("/root/dir"));
  base::FilePath bad_dir(FILE_PATH_LITERAL("/root"));
  base::FilePath good_file(FILE_PATH_LITERAL("/root/dir/good_file.txt"));
  base::FilePath bad_file(FILE_PATH_LITERAL("/root/dir/bad_file.txt"));

  url::Origin extension1_origin =
      url::Origin::Create(extensions::Extension::GetBaseURLFromExtensionId(
          "ddammdhioacbehjngdmkjcjbnfginlla"));
  url::Origin extension2_origin =
      url::Origin::Create(extensions::Extension::GetBaseURLFromExtensionId(
          "jkhdjkhkhsdkfhsdkhrterwmtermeter"));
  url::Origin app_origin = url::Origin::Create(GURL("chrome://file-manager"));

  FileAccessPermissions permissions;
  // By default extension have no access to any local file.
  EXPECT_FALSE(permissions.HasAccessPermission(extension1_origin, good_dir));
  EXPECT_FALSE(permissions.HasAccessPermission(extension1_origin, good_file));
  EXPECT_FALSE(permissions.HasAccessPermission(extension1_origin, bad_file));
  EXPECT_FALSE(permissions.HasAccessPermission(extension2_origin, good_dir));
  EXPECT_FALSE(permissions.HasAccessPermission(extension2_origin, good_file));
  EXPECT_FALSE(permissions.HasAccessPermission(extension2_origin, bad_file));
  EXPECT_FALSE(permissions.HasAccessPermission(app_origin, good_dir));
  EXPECT_FALSE(permissions.HasAccessPermission(app_origin, good_file));
  EXPECT_FALSE(permissions.HasAccessPermission(app_origin, bad_file));

  // After granting file access to the handler extension for a given file, it
  // can only access that file an nothing else.
  permissions.GrantAccessPermission(extension1_origin, good_file);
  EXPECT_FALSE(permissions.HasAccessPermission(extension1_origin, good_dir));
  EXPECT_TRUE(permissions.HasAccessPermission(extension1_origin, good_file));
  EXPECT_FALSE(permissions.HasAccessPermission(extension1_origin, bad_file));
  EXPECT_FALSE(permissions.HasAccessPermission(extension2_origin, good_dir));
  EXPECT_FALSE(permissions.HasAccessPermission(extension2_origin, good_file));
  EXPECT_FALSE(permissions.HasAccessPermission(extension2_origin, bad_file));
  EXPECT_FALSE(permissions.HasAccessPermission(app_origin, good_dir));
  EXPECT_FALSE(permissions.HasAccessPermission(app_origin, good_file));
  EXPECT_FALSE(permissions.HasAccessPermission(app_origin, bad_file));

  // After granting file access to the handler extension for a given directory,
  // it can access that directory and all files within it.
  permissions.GrantAccessPermission(extension2_origin, good_dir);
  EXPECT_FALSE(permissions.HasAccessPermission(extension1_origin, good_dir));
  EXPECT_TRUE(permissions.HasAccessPermission(extension1_origin, good_file));
  EXPECT_FALSE(permissions.HasAccessPermission(extension1_origin, bad_file));
  EXPECT_TRUE(permissions.HasAccessPermission(extension2_origin, good_dir));
  EXPECT_TRUE(permissions.HasAccessPermission(extension2_origin, good_file));
  EXPECT_TRUE(permissions.HasAccessPermission(extension2_origin, bad_file));
  EXPECT_FALSE(permissions.HasAccessPermission(app_origin, good_dir));
  EXPECT_FALSE(permissions.HasAccessPermission(app_origin, good_file));
  EXPECT_FALSE(permissions.HasAccessPermission(app_origin, bad_file));

  // Finally, after granting permission to the app for the given directory
  // it can access that director all all files within it.
  permissions.GrantAccessPermission(app_origin, good_dir);
  EXPECT_FALSE(permissions.HasAccessPermission(extension1_origin, good_dir));
  EXPECT_TRUE(permissions.HasAccessPermission(extension1_origin, good_file));
  EXPECT_FALSE(permissions.HasAccessPermission(extension1_origin, bad_file));
  EXPECT_TRUE(permissions.HasAccessPermission(extension2_origin, good_dir));
  EXPECT_TRUE(permissions.HasAccessPermission(extension2_origin, good_file));
  EXPECT_TRUE(permissions.HasAccessPermission(extension2_origin, bad_file));
  EXPECT_TRUE(permissions.HasAccessPermission(app_origin, good_dir));
  EXPECT_TRUE(permissions.HasAccessPermission(app_origin, good_file));
  EXPECT_TRUE(permissions.HasAccessPermission(app_origin, bad_file));

  // After revoking rights for extensions, they should not be able to access
  // any file system element anymore.
  permissions.RevokePermissions(extension1_origin);
  permissions.RevokePermissions(extension2_origin);
  permissions.RevokePermissions(app_origin);
  EXPECT_FALSE(permissions.HasAccessPermission(extension1_origin, good_dir));
  EXPECT_FALSE(permissions.HasAccessPermission(extension1_origin, good_file));
  EXPECT_FALSE(permissions.HasAccessPermission(extension1_origin, bad_file));
  EXPECT_FALSE(permissions.HasAccessPermission(extension2_origin, good_dir));
  EXPECT_FALSE(permissions.HasAccessPermission(extension2_origin, good_file));
  EXPECT_FALSE(permissions.HasAccessPermission(extension2_origin, bad_file));
  EXPECT_FALSE(permissions.HasAccessPermission(app_origin, good_dir));
  EXPECT_FALSE(permissions.HasAccessPermission(app_origin, good_file));
  EXPECT_FALSE(permissions.HasAccessPermission(app_origin, bad_file));
}

}  // namespace ash
