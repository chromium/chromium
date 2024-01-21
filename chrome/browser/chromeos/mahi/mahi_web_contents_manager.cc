// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/mahi/mahi_web_contents_manager.h"

#include "base/no_destructor.h"
#include "content/public/browser/web_contents.h"

namespace mahi {

namespace {
MahiWebContentsManager* g_mahi_web_content_manager_for_testing = nullptr;
}

// static
MahiWebContentsManager* MahiWebContentsManager::Get() {
  if (g_mahi_web_content_manager_for_testing) {
    return g_mahi_web_content_manager_for_testing;
  }
  static base::NoDestructor<MahiWebContentsManager> instance;
  return instance.get();
}

MahiWebContentsManager::MahiWebContentsManager() = default;

MahiWebContentsManager::~MahiWebContentsManager() = default;

void MahiWebContentsManager::OnFocusChanged(
    content::WebContents* web_contents) {
  // TODO(chenjih): handle the focus changes.
}

void MahiWebContentsManager::OnFocusedPageLoadComplete(
    content::WebContents* web_contents) {
  // TODO(chenjih): handle the focused page load completion.
}

// static
void MahiWebContentsManager::SetInstanceForTesting(
    MahiWebContentsManager* test_manager) {
  g_mahi_web_content_manager_for_testing = test_manager;
}

// static
void MahiWebContentsManager::ResetInstanceForTesting() {
  g_mahi_web_content_manager_for_testing = nullptr;
}

}  // namespace mahi
