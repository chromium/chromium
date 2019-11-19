// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fontconfig/fontconfig.h>
#include <sys/stat.h>
#include <time.h>
#include <utime.h>

#include <string>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/test/fontconfig_util_linux.h"

// GIANT WARNING: The point of this file is to front-load construction of the
// font cache [which takes 600ms] from test run time to compile time. This saves
// 600ms on each test shard which uses the font cache into compile time. The
// problem is that fontconfig cache construction is not intended to be
// deterministic. This executable tries to set some external state to ensure
// determinism. We have no way of guaranteeing that this produces correct
// results, or even has the intended effect.
int main() {
  // fontconfig generates a random uuid and uses it to match font folders with
  // the font cache. Rather than letting fontconfig generate a random uuid,
  // which introduces build non-determinism, we place a fixed uuid in the font
  // folder, which fontconfig will use to generate the cache.
  base::FilePath dir_module;
  base::PathService::Get(base::DIR_MODULE, &dir_module);
  base::FilePath uuid_file_path =
      dir_module.Append("test_fonts").Append(".uuid");
  const char uuid[] = "fb5c91b2895aa445d23aebf7f9e2189c";
  WriteFile(uuid_file_path, uuid, strlen(uuid));

  // fontconfig writes the mtime of the test_fonts directory into the cache. It
  // presumably checks this later to ensure that the cache is still up to date.
  // We set the mtime to an arbitrary, fixed time in the past.
  base::FilePath test_fonts_file_path = dir_module.Append("test_fonts");
  struct stat old_times;
  struct utimbuf new_times;

  stat(test_fonts_file_path.value().c_str(), &old_times);
  new_times.actime = old_times.st_atime;
  // Use an arbitrary, fixed time.
  new_times.modtime = 123456789;
  utime(test_fonts_file_path.value().c_str(), &new_times);

  base::FilePath fontconfig_caches = dir_module.Append("fontconfig_caches");

  // Delete directory before generating fontconfig caches. This will notify
  // future fontconfig_caches changes.
  CHECK(base::DeleteFileRecursively(fontconfig_caches));

  base::SetUpFontconfig();
  FcInit();
  FcFini();

  // Check existence of intended fontconfig cache file.
  CHECK(base::PathExists(
      fontconfig_caches.Append(base::StrCat({uuid, "-le64.cache-7"}))));
  return 0;
}
