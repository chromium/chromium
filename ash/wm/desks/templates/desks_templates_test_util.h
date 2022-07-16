// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_TEMPLATES_DESKS_TEMPLATES_TEST_UTIL_H_
#define ASH_WM_DESKS_TEMPLATES_DESKS_TEMPLATES_TEST_UTIL_H_

namespace views {
class Button;
}

namespace ash {

// These buttons are the ones on the primary root window.
views::Button* GetZeroStateDesksTemplatesButton();
views::Button* GetSaveDeskAsTemplateButton();

}  // namespace ash

#endif  // ASH_WM_DESKS_TEMPLATES_DESKS_TEMPLATES_TEST_UTIL_H_
