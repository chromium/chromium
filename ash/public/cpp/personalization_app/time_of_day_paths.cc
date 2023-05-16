// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/personalization_app/time_of_day_paths.h"

#include "base/no_destructor.h"

namespace ash::personalization_app {

namespace {

constexpr base::FilePath::CharType kAssetsRootDir[] =
    FILE_PATH_LITERAL("/usr/share/chromeos-assets/personalization/time_of_day");
constexpr base::FilePath::CharType kSrcSubDir[] = FILE_PATH_LITERAL("src");

}  // namespace

const base::FilePath& GetTimeOfDaySrcDir() {
  static const base::NoDestructor<base::FilePath> kPath(
      base::FilePath(kAssetsRootDir).Append(kSrcSubDir));
  return *kPath;
}

const base::FilePath::CharType kTimeOfDayCloudsVideo[] =
    FILE_PATH_LITERAL("clouds.webm");
const base::FilePath::CharType kTimeOfDayNewMexicoVideo[] =
    FILE_PATH_LITERAL("new_mexico.webm");
const base::FilePath::CharType kAmbientVideoHtml[] =
    FILE_PATH_LITERAL("ambient_video.html");

}  // namespace ash::personalization_app
