// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/browser_loader.h"

#include <utility>

#include "ash/constants/ash_switches.h"
#include "base/barrier_callback.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/values.h"
#include "chrome/browser/ash/crosapi/lacros_selection_loader.h"
#include "chrome/browser/ash/crosapi/lacros_selection_loader_factory.h"
#include "chrome/browser/ash/crosapi/stateful_lacros_loader.h"
#include "chromeos/ash/components/cryptohome/system_salt_getter.h"
#include "components/component_updater/ash/component_manager_ash.h"

namespace crosapi {

namespace {

class LacrosSelectionLoaderFactoryImpl : public LacrosSelectionLoaderFactory {
 public:
  explicit LacrosSelectionLoaderFactoryImpl(
      scoped_refptr<component_updater::ComponentManagerAsh> manager)
      : component_manager_(manager) {}

  LacrosSelectionLoaderFactoryImpl(const LacrosSelectionLoaderFactoryImpl&) =
      delete;
  LacrosSelectionLoaderFactoryImpl& operator=(
      const LacrosSelectionLoaderFactoryImpl&) = delete;

  ~LacrosSelectionLoaderFactoryImpl() override = default;

  std::unique_ptr<LacrosSelectionLoader> CreateStatefulLacrosLoader() override {
    return std::make_unique<StatefulLacrosLoader>(component_manager_);
  }

 private:
  scoped_refptr<component_updater::ComponentManagerAsh> component_manager_;
};

}  // namespace

BrowserLoader::BrowserLoader(
    scoped_refptr<component_updater::ComponentManagerAsh> manager)
    : factory_(std::make_unique<LacrosSelectionLoaderFactoryImpl>(manager)) {}

BrowserLoader::BrowserLoader(
    std::unique_ptr<LacrosSelectionLoaderFactory> factory)
    : factory_(std::move(factory)) {}

BrowserLoader::~BrowserLoader() = default;

void BrowserLoader::Unload() {
  // Can be called even if Lacros isn't enabled, to clean up the old install.
  // Unmount the stateful lacros-chrome if it was mounted.
  if (stateful_lacros_loader_) {
    stateful_lacros_loader_->Unload(base::BindOnce(
        &BrowserLoader::OnUnloadCompleted, weak_factory_.GetWeakPtr()));
  }
}

void BrowserLoader::OnUnloadCompleted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(stateful_lacros_loader_->IsUnloaded());
  stateful_lacros_loader_.reset();
  if (callback_on_unload_completion_) {
    std::move(callback_on_unload_completion_).Run();
  }
}

}  // namespace crosapi
