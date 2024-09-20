// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PALETTE_MOCK_PALETTE_TOOL_DELEGATE_H_
#define ASH_SYSTEM_PALETTE_MOCK_PALETTE_TOOL_DELEGATE_H_

#include "ash/system/palette/palette_tool.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

// Mock PaletteTool::Delegate class.
class MockPaletteToolDelegate : public PaletteTool::Delegate {
 public:
  MockPaletteToolDelegate();
  ~MockPaletteToolDelegate() override;

  MOCK_METHOD(void, EnableTool, (PaletteToolId tool_id), (override));
  MOCK_METHOD(void, DisableTool, (PaletteToolId tool_id), (override));
  MOCK_METHOD(void, HidePalette, (), (override));
  MOCK_METHOD(void, HidePaletteImmediately, (), (override));
  MOCK_METHOD(aura::Window*, GetWindow, (), (override));
};

}  // namespace ash

#endif  // ASH_SYSTEM_PALETTE_MOCK_PALETTE_TOOL_DELEGATE_H_
