// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FUCHSIA_FIDL_EVENT_HANDLER_H_
#define BASE_FUCHSIA_FIDL_EVENT_HANDLER_H_

#include <lib/fidl/cpp/wire/client_base.h>
#include <lib/fidl/cpp/wire/status.h>

#include <string>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/functional/callback.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace fidl {
template <typename Protocol>
class AsyncEventHandler;
}

namespace base {

// An implementation of `fidl::AsyncEventhandler` that simply DLOGs an error
// when `on_fidl_error` is called. The lifetime of an instance of this class
// needs to match the lifetime of the `fidl::Client` that it is used with.
template <typename Protocol>
class LoggingFidlErrorEventHandler : public fidl::AsyncEventHandler<Protocol> {
 public:
  explicit LoggingFidlErrorEventHandler(
      std::string protocol_name = fidl::DiscoverableProtocolName<Protocol>)
      : protocol_name_(std::move(protocol_name)) {}

  void on_fidl_error(fidl::UnbindInfo error) override {
    DLOG(ERROR) << protocol_name_ << " was disconnected with "
                << error.status_string() << ".";
  }

 private:
  std::string protocol_name_;
};

// An implementation of `fidl::AsyncEventHandler` that invokes the
// caller-supplied callback when `on_fidl_error` is called. The lifetime of an
// instance of this class needs to match the lifetime of the `fidl::Client` that
// it is used with.
template <typename Protocol>
class CallbackFidlErrorEventHandler : public fidl::AsyncEventHandler<Protocol> {
 public:
  using OnFidlErrorCallback = base::RepeatingCallback<void(fidl::UnbindInfo)>;

  CallbackFidlErrorEventHandler() = delete;
  explicit CallbackFidlErrorEventHandler(
      OnFidlErrorCallback on_fidl_error_callback)
      : on_fidl_error_callback_(std::move(on_fidl_error_callback)) {}

  void on_fidl_error(fidl::UnbindInfo error) override {
    on_fidl_error_callback_.Run(error);
  }

 private:
  OnFidlErrorCallback on_fidl_error_callback_;
};

}  // namespace base

#endif  // BASE_FUCHSIA_FIDL_EVENT_HANDLER_H_
