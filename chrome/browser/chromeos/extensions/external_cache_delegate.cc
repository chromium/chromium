// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/external_cache_delegate.h"

namespace chromeos {

void ExternalCacheDelegate::OnExtensionListsUpdated(
    const base::DictionaryValue* prefs) {}

void ExternalCacheDelegate::OnExtensionLoadedInCache(
    const extensions::ExtensionId& id) {}

void ExternalCacheDelegate::OnExtensionDownloadFailed(
    const extensions::ExtensionId& id) {}

void ExternalCacheDelegate::OnCachedExtensionFileDeleted(
    const extensions::ExtensionId& id) {}

}  // namespace chromeos
