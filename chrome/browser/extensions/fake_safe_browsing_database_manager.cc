// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/fake_safe_browsing_database_manager.h"

#include <stddef.h>

#include <algorithm>
#include <iterator>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "components/safe_browsing/core/db/util.h"
#include "components/safe_browsing/core/db/v4_protocol_manager_util.h"

namespace extensions {

FakeSafeBrowsingDatabaseManager::FakeSafeBrowsingDatabaseManager(bool enabled)
    : enabled_(enabled) {}

FakeSafeBrowsingDatabaseManager::~FakeSafeBrowsingDatabaseManager() {
}

FakeSafeBrowsingDatabaseManager& FakeSafeBrowsingDatabaseManager::Enable() {
  enabled_ = true;
  return *this;
}

FakeSafeBrowsingDatabaseManager& FakeSafeBrowsingDatabaseManager::Disable() {
  enabled_ = false;
  return *this;
}

FakeSafeBrowsingDatabaseManager&
FakeSafeBrowsingDatabaseManager::ClearUnsafe() {
  unsafe_ids_.clear();
  return *this;
}

FakeSafeBrowsingDatabaseManager& FakeSafeBrowsingDatabaseManager::SetUnsafe(
    const std::string& a) {
  ClearUnsafe();
  unsafe_ids_.insert(a);
  return *this;
}

FakeSafeBrowsingDatabaseManager& FakeSafeBrowsingDatabaseManager::SetUnsafe(
    const std::string& a, const std::string& b) {
  SetUnsafe(a);
  unsafe_ids_.insert(b);
  return *this;
}

FakeSafeBrowsingDatabaseManager& FakeSafeBrowsingDatabaseManager::SetUnsafe(
    const std::string& a, const std::string& b, const std::string& c) {
  SetUnsafe(a, b);
  unsafe_ids_.insert(c);
  return *this;
}

FakeSafeBrowsingDatabaseManager& FakeSafeBrowsingDatabaseManager::SetUnsafe(
    const std::string& a, const std::string& b, const std::string& c,
    const std::string& d) {
  SetUnsafe(a, b, c);
  unsafe_ids_.insert(d);
  return *this;
}

FakeSafeBrowsingDatabaseManager& FakeSafeBrowsingDatabaseManager::AddUnsafe(
    const std::string& a) {
  unsafe_ids_.insert(a);
  return *this;
}

FakeSafeBrowsingDatabaseManager& FakeSafeBrowsingDatabaseManager::RemoveUnsafe(
    const std::string& a) {
  unsafe_ids_.erase(a);
  return *this;
}

void FakeSafeBrowsingDatabaseManager::NotifyUpdate() {
  NotifyDatabaseUpdateFinished();
}

bool FakeSafeBrowsingDatabaseManager::CheckExtensionIDs(
    const std::set<std::string>& extension_ids,
    Client* client) {
  if (!enabled_)
    return true;

  std::set<safe_browsing::FullHash> unsafe_extension_ids;
  for (const auto& extension_id : extension_ids) {
    if (unsafe_ids_.count(extension_id))
      unsafe_extension_ids.insert(extension_id);
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &SafeBrowsingDatabaseManager::Client::OnCheckExtensionsResult,
          base::Unretained(client), unsafe_extension_ids));
  return false;
}

}  // namespace extensions
