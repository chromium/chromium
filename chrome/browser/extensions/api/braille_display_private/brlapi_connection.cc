// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/extensions/api/braille_display_private/brlapi_connection.h"

#include <errno.h>

#include <string>

#include "base/files/file_descriptor_watcher_posix.h"
#include "base/logging.h"
#include "base/memory/free_deleter.h"
#include "base/memory/raw_ptr.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

namespace extensions {
namespace api {
namespace braille_display_private {

namespace {
// Default virtual terminal.  This can be overriden by setting the
// WINDOWPATH environment variable.  This is only used when not running
// under Chrome OS (that is in aura for a Linux desktop).
// TODO(plundblad): Find a way to detect the controlling terminal of the
// X server.
static const int kDefaultTtyLinux = 7;
#if BUILDFLAG(IS_CHROMEOS_ASH)
// The GUI is always running on vt1 in Chrome OS.
static const int kDefaultTtyChromeOS = 1;
#endif
}  // namespace

class BrlapiConnectionImpl : public BrlapiConnection {
 public:
  explicit BrlapiConnectionImpl(LibBrlapiLoader* loader) :
      libbrlapi_loader_(loader) {}
  BrlapiConnectionImpl(const BrlapiConnectionImpl&) = delete;
  BrlapiConnectionImpl& operator=(const BrlapiConnectionImpl&) = delete;
  ~BrlapiConnectionImpl() override { Disconnect(); }

  ConnectResult Connect(OnDataReadyCallback on_data_ready) override;
  void Disconnect() override;
  bool Connected() override { return handle_ != nullptr; }
  brlapi_error_t* BrlapiError() override;
  std::string BrlapiStrError() override;
  bool GetDisplaySize(unsigned int* rows, unsigned int* columns) override;
  bool WriteDots(const std::vector<unsigned char>& cells) override;
  int ReadKey(brlapi_keyCode_t* keyCode) override;
  bool GetCellSize(unsigned int* cell_size) override;

 private:
  bool CheckConnected();
  ConnectResult ConnectResultForError();

  raw_ptr<LibBrlapiLoader> libbrlapi_loader_;
  std::unique_ptr<brlapi_handle_t, base::FreeDeleter> handle_;
  std::unique_ptr<base::FileDescriptorWatcher::Controller> fd_controller_;
};

std::unique_ptr<BrlapiConnection> BrlapiConnection::Create(
    LibBrlapiLoader* loader) {
  DCHECK(loader->loaded());
  return std::unique_ptr<BrlapiConnection>(new BrlapiConnectionImpl(loader));
}

BrlapiConnection::ConnectResult BrlapiConnectionImpl::Connect(
    OnDataReadyCallback on_data_ready) {
  DCHECK(!handle_);
  handle_.reset(reinterpret_cast<brlapi_handle_t*>(
      malloc(libbrlapi_loader_->brlapi_getHandleSize())));
  int fd = libbrlapi_loader_->brlapi__openConnection(handle_.get(), nullptr,
                                                     nullptr);
  if (fd < 0) {
    handle_.reset();
    VLOG(1) << "Error connecting to brlapi: " << BrlapiStrError();
    return ConnectResultForError();
  }
  int path[2] = {0, 0};
  int pathElements = 0;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (base::SysInfo::IsRunningOnChromeOS())
    path[pathElements++] = kDefaultTtyChromeOS;
#endif
  if (pathElements == 0 && getenv("WINDOWPATH") == nullptr)
    path[pathElements++] = kDefaultTtyLinux;
  if (libbrlapi_loader_->brlapi__enterTtyModeWithPath(
          handle_.get(), path, pathElements, nullptr) < 0) {
    LOG(ERROR) << "brlapi: couldn't enter tty mode: " << BrlapiStrError();
    Disconnect();
    return CONNECT_ERROR_RETRY;
  }
  unsigned int rows = 0;
  unsigned int columns = 0;

  if (!GetDisplaySize(&rows, &columns)) {
    // Error already logged.
    Disconnect();
    return CONNECT_ERROR_RETRY;
  }

  // A display size of 0 means no display connected.  We can't reliably
  // detect when a display gets connected, so fail and let the caller
  // retry connecting.
  if (rows * columns == 0) {
    VLOG(1) << "No braille display connected";
    Disconnect();
    return CONNECT_ERROR_RETRY;
  }

  const brlapi_keyCode_t extraKeys[] = {
      BRLAPI_KEY_TYPE_CMD | BRLAPI_KEY_CMD_OFFLINE,
      // brltty 5.1 converts dot input to Unicode characters unless we
      // explicitly accept this command.
      BRLAPI_KEY_TYPE_CMD | BRLAPI_KEY_CMD_PASSDOTS,
  };
  if (libbrlapi_loader_->brlapi__acceptKeys(handle_.get(),
                                            brlapi_rangeType_command, extraKeys,
                                            std::size(extraKeys)) < 0) {
    LOG(ERROR) << "Couldn't acceptKeys: " << BrlapiStrError();
    Disconnect();
    return CONNECT_ERROR_RETRY;
  }

  fd_controller_ =
      base::FileDescriptorWatcher::WatchReadable(fd, std::move(on_data_ready));

  return CONNECT_SUCCESS;
}

void BrlapiConnectionImpl::Disconnect() {
  if (!handle_) {
    return;
  }
  fd_controller_.reset();
  libbrlapi_loader_->brlapi__closeConnection(
      handle_.get());
  handle_.reset();
}

brlapi_error_t* BrlapiConnectionImpl::BrlapiError() {
  return libbrlapi_loader_->brlapi_error_location();
}

std::string BrlapiConnectionImpl::BrlapiStrError() {
  return libbrlapi_loader_->brlapi_strerror(BrlapiError());
}

bool BrlapiConnectionImpl::GetDisplaySize(unsigned int* columns,
                                          unsigned int* rows) {
  if (!CheckConnected()) {
    return false;
  }
  if (libbrlapi_loader_->brlapi__getDisplaySize(handle_.get(), columns, rows) <
      0) {
    LOG(ERROR) << "Couldn't get braille display size " << BrlapiStrError();
    return false;
  }
  return true;
}

bool BrlapiConnectionImpl::WriteDots(const std::vector<unsigned char>& cells) {
  // Cells is a 2D vector, compressed into 1D.
  if (!CheckConnected())
    return false;
  if (libbrlapi_loader_->brlapi__writeDots(handle_.get(), cells.data()) < 0) {
    VLOG(1) << "Couldn't write to brlapi: " << BrlapiStrError();
    return false;
  }
  return true;
}

int BrlapiConnectionImpl::ReadKey(brlapi_keyCode_t* key_code) {
  if (!CheckConnected())
    return -1;
  return libbrlapi_loader_->brlapi__readKey(
      handle_.get(), 0 /*wait*/, key_code);
}

bool BrlapiConnectionImpl::GetCellSize(unsigned int* cell_size) {
  if (!CheckConnected()) {
    return false;
  }

  brlapi_param_deviceCellSize_t device_cell_size;
  ssize_t result = libbrlapi_loader_->brlapi__getParameter(
      handle_.get(), BRLAPI_PARAM_DEVICE_CELL_SIZE, 0, BRLAPI_PARAMF_GLOBAL,
      &device_cell_size, sizeof(device_cell_size));

  if (result == -1 || result != sizeof(device_cell_size))
    return false;

  *cell_size = device_cell_size;
  return true;
}

bool BrlapiConnectionImpl::CheckConnected() {
  if (!handle_) {
    BrlapiError()->brlerrno = BRLAPI_ERROR_ILLEGAL_INSTRUCTION;
    return false;
  }
  return true;
}

BrlapiConnection::ConnectResult BrlapiConnectionImpl::ConnectResultForError() {
  const brlapi_error_t* error = BrlapiError();
  // For the majority of users, the socket file will never exist because
  // the daemon is never run.  Avoid retrying in this case, relying on
  // the socket directory to change and trigger further tries if the
  // daemon comes up later on.
  if (error->brlerrno == BRLAPI_ERROR_LIBCERR
      && error->libcerrno == ENOENT) {
    return CONNECT_ERROR_NO_RETRY;
  }
  return CONNECT_ERROR_RETRY;
}

}  // namespace braille_display_private
}  // namespace api
}  // namespace extensions
