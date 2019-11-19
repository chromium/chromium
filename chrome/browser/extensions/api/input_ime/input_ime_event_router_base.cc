// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/input_ime/input_ime_event_router_base.h"

#include "base/strings/string_number_conversions.h"

namespace extensions {

InputImeEventRouterBase::InputImeEventRouterBase(Profile* profile)
    : profile_(profile) {}

InputImeEventRouterBase::~InputImeEventRouterBase() = default;

}  // namespace extensions
