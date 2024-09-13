// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ON_DEVICE_TRANSLATION_CONSTANTS_H_
#define CHROME_BROWSER_ON_DEVICE_TRANSLATION_CONSTANTS_H_

#include "base/files/file_path.h"

namespace on_device_translation {

// The root directory of the TranslateKit relative to the User Data directory.
// This is the parent directory of
// `kTranslateKitComponentInstallationRelativePath` and its model components.
extern const base::FilePath::CharType
    kTranslateKitRootInstallationRelativeDir[];

// The installation location of the TranslateKit binary component relative to
// the User Data directory.
extern const base::FilePath::CharType
    kTranslateKitBinaryInstallationRelativeDir[];

// The installation location of the TranslateKit langaage package component
// relative to the User Data directory.
extern const base::FilePath::CharType
    kTranslateKitLanguagePackInstallationRelativeDir[];

// Returns the absolute path of the TranslateKit root directory.
const base::FilePath GetTranslateKitRootDirectory();

}  // namespace on_device_translation

#endif  // CHROME_BROWSER_ON_DEVICE_TRANSLATION_CONSTANTS_H_
