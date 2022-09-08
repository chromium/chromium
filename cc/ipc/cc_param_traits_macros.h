// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_IPC_CC_PARAM_TRAITS_MACROS_H_
#define CC_IPC_CC_PARAM_TRAITS_MACROS_H_

#include "base/component_export.h"
#include "cc/input/browser_controls_state.h"
#include "cc/input/overscroll_behavior.h"
#include "cc/input/touch_action.h"
#include "cc/trees/browser_controls_params.h"
#include "ipc/ipc_message_macros.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT COMPONENT_EXPORT(CC_IPC)

IPC_ENUM_TRAITS_MAX_VALUE(cc::OverscrollBehavior::Type,
                          cc::OverscrollBehavior::Type::kMax)

IPC_STRUCT_TRAITS_BEGIN(cc::OverscrollBehavior)
  IPC_STRUCT_TRAITS_MEMBER(x)
  IPC_STRUCT_TRAITS_MEMBER(y)
IPC_STRUCT_TRAITS_END()

IPC_ENUM_TRAITS_MAX_VALUE(cc::TouchAction, cc::TouchAction::kMax)

IPC_STRUCT_TRAITS_BEGIN(cc::BrowserControlsParams)
  IPC_STRUCT_TRAITS_MEMBER(top_controls_height)
  IPC_STRUCT_TRAITS_MEMBER(top_controls_min_height)
  IPC_STRUCT_TRAITS_MEMBER(bottom_controls_height)
  IPC_STRUCT_TRAITS_MEMBER(bottom_controls_min_height)
  IPC_STRUCT_TRAITS_MEMBER(animate_browser_controls_height_changes)
  IPC_STRUCT_TRAITS_MEMBER(browser_controls_shrink_blink_size)
  IPC_STRUCT_TRAITS_MEMBER(only_expand_top_controls_at_page_top)
IPC_STRUCT_TRAITS_END()

IPC_ENUM_TRAITS_MAX_VALUE(cc::BrowserControlsState,
                          cc::BrowserControlsState::kMaxValue)

#endif  // CC_IPC_CC_PARAM_TRAITS_MACROS_H_
