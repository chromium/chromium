// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/external_cache_delegate.h"

#include "base/values.h"

namespace chromeos {

void ExternalCacheDelegate::OnExtensionListsUpdated(
    const base::Value::Dict& prefs) {}

void ExternalCacheDelegate::OnExtensionLoadedInCache(
    const extensions::ExtensionId& id) {}

void ExternalCacheDelegate::OnExtensionDownloadFailed(
    const extensions::ExtensionId& id) {}

void ExternalCacheDelegate::OnCachedExtensionFileDeleted(
    const extensions::ExtensionId& id) {}

}  // namespace chromeos
