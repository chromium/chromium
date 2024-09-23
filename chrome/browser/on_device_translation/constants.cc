// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/on_device_translation/constants.h"

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "components/component_updater/component_updater_paths.h"

namespace on_device_translation {

// The installation location of the TranslateKit binary component relative to
// the User Data directory.
constexpr base::FilePath::CharType
    kTranslateKitBinaryInstallationRelativeDir[] =
        FILE_PATH_LITERAL("TranslateKit/lib");

constexpr base::FilePath::CharType
    kTranslateKitLanguagePackInstallationRelativeDir[] =
        FILE_PATH_LITERAL("TranslateKit/models");

}  // namespace on_device_translation
