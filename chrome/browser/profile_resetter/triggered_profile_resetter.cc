// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profile_resetter/triggered_profile_resetter.h"

#include "base/check.h"
#include "build/build_config.h"

TriggeredProfileResetter::TriggeredProfileResetter(Profile* profile)
#if BUILDFLAG(IS_WIN)
    : profile_(profile)
#endif  // BUILDFLAG(IS_WIN)
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
