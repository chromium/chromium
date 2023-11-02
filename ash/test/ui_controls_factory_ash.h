// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_UI_CONTROLS_FACTORY_ASH_H_
#define ASH_TEST_UI_CONTROLS_FACTORY_ASH_H_

namespace ui_controls {
class UIControlsAura;
}

namespace ash {
namespace test {

ui_controls::UIControlsAura* CreateAshUIControls();

}  // namespace test
}  // namespace ash

#endif  // ASH_TEST_UI_CONTROLS_FACTORY_ASH_H_
