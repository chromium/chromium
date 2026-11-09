// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/base/histograms.h"

#include <string_view>

#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"

namespace cc {

namespace {

// Global data tracking the client name that was set.
// Both of these variables are protected by `GetClientNameLock()`.
static const char* g_client_name = nullptr;
static bool g_multiple_client_names_set = false;

base::Lock& GetClientNameLock() {
  static base::NoDestructor<base::Lock> client_name_lock;
  return *client_name_lock;
}

}  // namespace

void SetClientNameForMetrics(const char* client_name) {
  base::AutoLock auto_lock(GetClientNameLock());

  // Only warn once.
  if (g_multiple_client_names_set)
    return;

  // If a different name is set, return nullptr from now on and log a warning.
  const char* old_client_name = g_client_name;
  if (old_client_name && (std::string_view(old_client_name) != client_name)) {
    g_client_name = nullptr;
    g_multiple_client_names_set = true;
    LOG(WARNING) << "Started multiple compositor clients (" << old_client_name
                 << ", " << client_name
                 << ") in one process. Some metrics will be disabled.";
    return;
  }

  // If the client name is being set for the first time, store it.
  if (!old_client_name)
    g_client_name = client_name;
}

const char* GetClientNameForMetrics() {
  base::AutoLock auto_lock(GetClientNameLock());
  return g_client_name;
}

}  // namespace cc
