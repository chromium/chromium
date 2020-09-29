// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/lacros_chrome_service_delegate_impl.h"

#include "base/logging.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/common/channel_info.h"

LacrosChromeServiceDelegateImpl::LacrosChromeServiceDelegateImpl() = default;

LacrosChromeServiceDelegateImpl::~LacrosChromeServiceDelegateImpl() = default;

void LacrosChromeServiceDelegateImpl::NewWindow() {
  // TODO(crbug.com/1102815): Find what profile should be used.
  Profile* profile = ProfileManager::GetLastUsedProfileAllowedByPolicy();
  DCHECK(profile) << "No last used profile is found.";
  chrome::NewEmptyWindow(profile);
}

std::string LacrosChromeServiceDelegateImpl::GetChromeVersion() {
  return chrome::GetVersionString();
}
