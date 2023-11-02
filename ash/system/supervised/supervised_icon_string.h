// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_SUPERVISED_SUPERVISED_ICON_STRING_H_
#define ASH_SYSTEM_SUPERVISED_SUPERVISED_ICON_STRING_H_

#include <string>


namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace ash {

const gfx::VectorIcon& GetSupervisedUserIcon();

std::u16string GetSupervisedUserMessage();

}  // namespace ash

#endif  // ASH_SYSTEM_SUPERVISED_SUPERVISED_ICON_STRING_H_
