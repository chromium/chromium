// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/external_loader.h"

#include "base/check_op.h"
#include "base/values.h"
#include "chrome/browser/extensions/external_provider_impl.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace extensions {

ExternalLoader::ExternalLoader() = default;

void ExternalLoader::Init(ExternalProviderImpl* owner) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  owner_ = owner;
}

const base::FilePath ExternalLoader::GetBaseCrxFilePath() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // By default, relative paths are not supported.
  // Subclasses that wish to support them should override this method.
  return base::FilePath();
}

void ExternalLoader::OwnerShutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  owner_ = nullptr;
}

ExternalLoader::~ExternalLoader() = default;

void ExternalLoader::LoadFinished(
    std::unique_ptr<base::DictionaryValue> prefs) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (owner_)
    owner_->SetPrefs(std::move(prefs));
}

void ExternalLoader::OnUpdated(
    std::unique_ptr<base::DictionaryValue> updated_prefs) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (owner_)
    owner_->UpdatePrefs(std::move(updated_prefs));
}

}  // namespace extensions
