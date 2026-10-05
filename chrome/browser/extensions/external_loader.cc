// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/external_loader.h"

#include "base/check_op.h"
#include "base/values.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/external_provider_interface.h"
#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

using content::BrowserThread;

namespace extensions {

ExternalLoader::ExternalLoader() = default;

void ExternalLoader::Init(ExternalProviderInterface* owner) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);
  owner_ = owner;
}

const base::FilePath ExternalLoader::GetBaseCrxFilePath() {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);

  // By default, relative paths are not supported.
  // Subclasses that wish to support them should override this method.
  return base::FilePath();
}

void ExternalLoader::OwnerShutdown() {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);
  owner_ = nullptr;
}

ExternalLoader::~ExternalLoader() = default;

void ExternalLoader::LoadFinished(base::DictValue prefs) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);
  if (owner_)
    owner_->SetPrefs(std::move(prefs));
}

void ExternalLoader::OnUpdated(base::DictValue updated_prefs) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);
  if (owner_)
    owner_->UpdatePrefs(std::move(updated_prefs));
}

}  // namespace extensions
