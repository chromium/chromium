// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_dev_zero_fuchsia.h"

#include <fuchsia/io/cpp/fidl.h>
#include <lib/fdio/namespace.h>
#include <lib/fidl/cpp/interface_request.h>
#include <lib/vfs/cpp/pseudo_dir.h>
#include <lib/vfs/cpp/vmo_file.h>
#include <lib/zx/channel.h>
#include <lib/zx/vmo.h>
#include <stdint.h>
#include <zircon/types.h>

#include <functional>
#include <utility>

#include "base/check.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"

namespace base {

// ScopedDevZero::Server -------------------------------------------------------

// A helper that lives on a dedicated thread, serving up a pesudo-dir containing
// a "zero" file.
class ScopedDevZero::Server {
 public:
  // Creates the pseudo-dir representing /dev as `directory_request` and serves
  // up a "zero" file within it. `on_initialized` is run with the status.
  Server(fidl::InterfaceRequest<fuchsia::io::Directory> directory_request,
         OnceCallback<void(zx_status_t status)> on_initialized);
  Server(const Server&) = delete;
  Server& operator=(const Server&) = delete;
  ~Server() = default;

 private:
  vfs::PseudoDir dev_dir_;
};

ScopedDevZero::Server::Server(
    fidl::InterfaceRequest<fuchsia::io::Directory> directory_request,
    OnceCallback<void(zx_status_t status)> on_initialized) {
  // VMOs are filled with zeros at construction, so create a big one and serve
  // it as "zero" within the given `directory_request`. All virtual pages in the
  // VMO are backed by the singular physical "zero page", so no memory is
  // allocated until a write occurs (which will never happen). On the server
  // end, the VMO should not take up address space on account of never being
  // mapped. On the read side (libfdio) it may get mapped, but only for the size
  // of a given read - it may also just use the zx_vmo_read syscall to avoid
  // ever needing to map it.
  zx::vmo vmo;
  auto status = zx::vmo::create(/*size=*/UINT32_MAX, /*options=*/0, &vmo);
  ZX_LOG_IF(ERROR, status != ZX_OK, status);

  if (status == ZX_OK) {
    status = dev_dir_.AddEntry(
        "zero",
        std::make_unique<vfs::VmoFile>(std::move(vmo), /*length=*/UINT32_MAX));
    ZX_LOG_IF(ERROR, status != ZX_OK, status);
  }

  if (status == ZX_OK) {
    status = dev_dir_.Serve(fuchsia::io::OpenFlags::RIGHT_READABLE,
                            directory_request.TakeChannel());
    ZX_LOG_IF(ERROR, status != ZX_OK, status);
  }

  std::move(on_initialized).Run(status);
}

// ScopedDevZero ---------------------------------------------------------------

// static
ScopedDevZero* ScopedDevZero::instance_ = nullptr;

// static
scoped_refptr<ScopedDevZero> ScopedDevZero::Get() {
  if (instance_)
    return WrapRefCounted(instance_);
  scoped_refptr<ScopedDevZero> result = AdoptRef(new ScopedDevZero);
  return result->Initialize() ? std::move(result) : nullptr;
}

ScopedDevZero::ScopedDevZero() : io_thread_("/dev/zero") {
  DCHECK_EQ(instance_, nullptr);
  instance_ = this;
}

ScopedDevZero::~ScopedDevZero() {
  DCHECK_EQ(instance_, this);
  if (global_namespace_)
    fdio_ns_unbind(std::exchange(global_namespace_, nullptr), "/dev");
  instance_ = nullptr;
}

bool ScopedDevZero::Initialize() {
  auto status = fdio_ns_get_installed(&global_namespace_);
  if (status != ZX_OK) {
    ZX_LOG(ERROR, status);
    return false;
  }

  if (!io_thread_.StartWithOptions(Thread::Options(MessagePumpType::IO, 0)))
    return false;

  zx::channel client;
  zx::channel request;
  status = zx::channel::create(0, &client, &request);
  ZX_CHECK(status == ZX_OK, status);

  RunLoop run_loop;
  server_ = SequenceBound<Server>(
      io_thread_.task_runner(),
      fidl::InterfaceRequest<fuchsia::io::Directory>(std::move(request)),
      base::BindOnce(
          [](base::OnceClosure quit_loop, zx_status_t& status,
             zx_status_t init_status) {
            status = init_status;
            std::move(quit_loop).Run();
          },
          run_loop.QuitClosure(), std::ref(status)));
  run_loop.Run();

  if (status != ZX_OK)
    return false;

  // Install the directory holding "zero" into the global namespace as /dev.
  // This relies on the component not asking for any /dev entries in its
  // manifest, as nested namespaces are not allowed.
  status = fdio_ns_bind(global_namespace_, "/dev", client.release());
  if (status != ZX_OK) {
    ZX_LOG(ERROR, status);
    global_namespace_ = nullptr;
    server_.Reset();
    return false;
  }

  return true;
}

}  // namespace base
