// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/fuchsia/scoped_service_binding.h"

#include <lib/sys/cpp/outgoing_directory.h>
#include <lib/vfs/cpp/pseudo_dir.h>
#include <lib/vfs/cpp/service.h>

namespace base {
namespace fuchsia {
namespace internal {

ScopedServiceBindingBase::ScopedServiceBindingBase(
    sys::OutgoingDirectory* outgoing_directory)
    : ScopedServiceBindingBase(
          outgoing_directory->GetOrCreateDirectory("svc")) {}

ScopedServiceBindingBase::ScopedServiceBindingBase(vfs::PseudoDir* pseudo_dir)
    : pseudo_dir_(pseudo_dir) {}

ScopedServiceBindingBase::~ScopedServiceBindingBase() = default;

void ScopedServiceBindingBase::RegisterService(const char* service_name,
                                               Connector connector) {
  pseudo_dir_->AddEntry(service_name,
                        std::make_unique<vfs::Service>(std::move(connector)));
}

void ScopedServiceBindingBase::UnregisterService(const char* service_name) {
  pseudo_dir_->RemoveEntry(service_name);
}

}  // namespace internal
}  // namespace fuchsia
}  // namespace base
