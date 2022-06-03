// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profile_resetter/triggered_profile_resetter.h"

#include "base/check.h"

TriggeredProfileResetter::TriggeredProfileResetter(Profile* profile)
#if defined(OS_WIN)
    : profile_(profile)
#endif  // defined(OS_WIN)
{
}

TriggeredProfileResetter::~TriggeredProfileResetter() {}

bool TriggeredProfileResetter::HasResetTrigger() {
  DCHECK(activate_called_);
  return has_reset_trigger_;
}

void TriggeredProfileResetter::ClearResetTrigger() {
  has_reset_trigger_ = false;
}

std::u16string TriggeredProfileResetter::GetResetToolName() {
  return tool_name_;
}
