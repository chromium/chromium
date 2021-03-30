// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FUCHSIA_SCOPED_SERVICE_PUBLISHER_H_
#define BASE_FUCHSIA_SCOPED_SERVICE_PUBLISHER_H_

#include <lib/async/dispatcher.h>
#include <lib/fidl/cpp/interface_request.h>
#include <lib/sys/cpp/outgoing_directory.h>
#include <lib/vfs/cpp/pseudo_dir.h>
#include <lib/vfs/cpp/service.h>
#include <lib/zx/channel.h>

#include "base/base_export.h"
#include "base/macros.h"
#include "base/strings/string_piece.h"

namespace base {

template <typename Interface>
class BASE_EXPORT ScopedServicePublisher {
 public:
  // Publishes a public service in the specified |outgoing_directory|.
  // |outgoing_directory| and |handler| must outlive the binding.
  ScopedServicePublisher(sys::OutgoingDirectory* outgoing_directory,
                         fidl::InterfaceRequestHandler<Interface> handler,
                         base::StringPiece name = Interface::Name_)
      : ScopedServicePublisher(outgoing_directory->GetOrCreateDirectory("svc"),
                               std::move(handler), name) {}

  // Publishes a service in the specified |pseudo_dir|. |pseudo_dir| and
  // |handler| must outlive the binding.
  ScopedServicePublisher(vfs::PseudoDir* pseudo_dir,
                         fidl::InterfaceRequestHandler<Interface> handler,
                         base::StringPiece name = Interface::Name_)
      : pseudo_dir_(pseudo_dir), name_(name) {
    pseudo_dir_->AddEntry(name_,
                          std::make_unique<vfs::Service>(std::move(handler)));
  }

  ~ScopedServicePublisher() { pseudo_dir_->RemoveEntry(name_); }

 private:
  vfs::PseudoDir* const pseudo_dir_ = nullptr;
  std::string name_;
  DISALLOW_COPY_AND_ASSIGN(ScopedServicePublisher);
};

}  // namespace base

#endif  // BASE_FUCHSIA_SCOPED_SERVICE_PUBLISHER_H_
