// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/notice/desktop_entrypoint_handlers.h"

#include "chrome/browser/privacy_sandbox/notice/desktop_view_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/navigation_handle.h"

namespace privacy_sandbox {
//-----------------------------------------------------------------------------
// EntryPointHandler
//-----------------------------------------------------------------------------
EntryPointHandler::EntryPointHandler(
    base::RepeatingCallback<void()> entry_point_callback)
    : entry_point_callback_(std::move(entry_point_callback)) {}
EntryPointHandler::~EntryPointHandler() = default;

void EntryPointHandler::HandleEntryPoint() {
  entry_point_callback_.Run();
}

//-----------------------------------------------------------------------------
// NavigationHandler
//-----------------------------------------------------------------------------
NavigationHandler::NavigationHandler(
    base::RepeatingCallback<void()> entry_point_callback)
    : EntryPointHandler(std::move(entry_point_callback)) {}

void NavigationHandler::HandleNewNavigation(
    content::NavigationHandle* navigation_handle,
    Profile* profile) {
  // TODO(crbug.com/408016824): Implement to perform checks needed before
  // showing notice.
  HandleEntryPoint();
}

}  // namespace privacy_sandbox
