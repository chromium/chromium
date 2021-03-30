// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants related to ChromeOS.

#ifndef ASH_CONSTANTS_ASH_CONSTANTS_H_
#define ASH_CONSTANTS_ASH_CONSTANTS_H_

#include "base/component_export.h"
#include "base/files/file_path.h"

namespace ash {

COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::FilePath::CharType kDriveCacheDirname[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::FilePath::CharType kNssCertDbPath[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::FilePath::CharType kNssKeyDbPath[];

}  // namespace ash

#endif  // ASH_CONSTANTS_ASH_CONSTANTS_H_
