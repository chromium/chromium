// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/https_only_mode_tab_storage.h"

#include <memory>

#include "content/public/browser/web_contents.h"

// This bit of chaos ensures that kHttpsOnlyModeStorageKey is an arbitrary but
// unique-in-the-process value (namely, its own memory address) without casts.
const void* const kHttpsOnlyModeStorageKey = &kHttpsOnlyModeStorageKey;

HttpsOnlyModeTabStorage::HttpsOnlyModeTabStorage() = default;
HttpsOnlyModeTabStorage::~HttpsOnlyModeTabStorage() = default;

// static
HttpsOnlyModeTabStorage* HttpsOnlyModeTabStorage::GetOrCreate(
    content::WebContents* web_contents) {
  auto* existing_storage = static_cast<HttpsOnlyModeTabStorage*>(
      web_contents->GetUserData(kHttpsOnlyModeStorageKey));
  if (existing_storage) {
    return existing_storage;
  }

  auto new_storage = std::make_unique<HttpsOnlyModeTabStorage>();
  auto* new_storage_ptr = new_storage.get();
  web_contents->SetUserData(kHttpsOnlyModeStorageKey, std::move(new_storage));
  return new_storage_ptr;
}

void HttpsOnlyModeTabStorage::AddHostToAllowlist(const std::string& hostname) {
  allowlist_.insert(hostname);
}

bool HttpsOnlyModeTabStorage::IsHostAllowlisted(const std::string& hostname) {
  auto it = allowlist_.find(hostname);
  return it != allowlist_.end();
}
