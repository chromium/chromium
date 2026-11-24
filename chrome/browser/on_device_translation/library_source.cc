// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/on_device_translation/library_source.h"

#include <utility>

#include "base/check.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/on_device_translation/component_updater_library_source.h"
#include "components/on_device_translation/features.h"

namespace on_device_translation {

// static
std::unique_ptr<TranslateKitLibrarySource> TranslateKitLibrarySource::Create() {
  base::FilePath library_path = GetTranslateKitBinaryPathFromCommandLine();
  if (!library_path.empty()) {
    return std::make_unique<FixedPathLibrarySource>(std::move(library_path));
  }
  return std::make_unique<ComponentUpdaterLibrarySource>(
      g_browser_process->local_state());
}

void TranslateKitLibrarySource::SetClient(Client* client) {
  CHECK(!client_);
  CHECK(client);
  client_ = client;
}

bool TranslateKitLibrarySource::IsSupported() const {
  return true;
}

FixedPathLibrarySource::FixedPathLibrarySource(base::FilePath library_path)
    : library_path_(std::move(library_path)) {}

FixedPathLibrarySource::~FixedPathLibrarySource() = default;

bool FixedPathLibrarySource::IsSupported() const {
  return !library_path_.empty();
}

base::FilePath FixedPathLibrarySource::GetLibraryPath() const {
  return library_path_;
}

void FixedPathLibrarySource::Install() {
  // The library is installed if the path is set. Otherwise it never will be,
  // and `OnDeviceTranslationInstallerImpl::Init()` fails before it gets here.
}

}  // namespace on_device_translation
