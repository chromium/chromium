// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/fuchsia/fidlgen_js/runtime/zircon.h"

#include <lib/async/default.h>
#include <lib/async/wait.h>
#include <lib/zx/channel.h>
#include <zircon/errors.h>
#include <zircon/syscalls.h>
#include <zircon/types.h>

#include "base/bind.h"
#include "base/threading/thread_checker.h"
#include "gin/arguments.h"
#include "gin/array_buffer.h"
#include "gin/converter.h"
#include "gin/data_object_builder.h"
#include "gin/function_template.h"
#include "gin/public/gin_embedders.h"

namespace {

fidljs::WaitSet& GetWaitsForIsolate(v8::Isolate* isolate) {
  return *static_cast<fidljs::WaitSet*>(
      isolate->GetData(gin::kEmbedderFuchsia));
}

}  // namespace

namespace fidljs {

class WaitPromiseImpl : public async_wait_t {
 public:
  WaitPromiseImpl(v8::Isolate* isolate,
                  v8::Local<v8::Context> context,
                  v8::Local<v8::Promise::Resolver> resolver,
                  zx_handle_t handle,
                  zx_signals_t signals)
      : async_wait_t({ASYNC_STATE_INIT, &WaitPromiseImpl::StaticOnSignaled,
                      handle, signals, 0}),
        isolate_(isolate),
        wait_state_(WaitState::kCreated),
        failed_start_status_(ZX_OK) {
    context_.Reset(isolate_, context);
    resolver_.Reset(isolate_, resolver);
  }

  ~WaitPromiseImpl() {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    switch (wait_state_) {
      case WaitState::kCreated:
        // The wait never started, so reject the promise (but don't attempt to
        // cancel the wait).
        DCHECK_NE(failed_start_status_, ZX_OK);
        RejectPromise(failed_start_status_, 0);
        break;

      case WaitState::kStarted:
        // The wait was started, but has not yet completed. Cancel the wait and
        // reject the promise. The object is being destructed here because it's
        // been removed from the set of waits attached to the isolate, so
        // we need not remove it.
        CHECK_EQ(async_cancel_wait(async_get_default_dispatcher(), this),
                 ZX_OK);
        RejectPromise(ZX_ERR_CANCELED, 0);
        break;

      case WaitState::kCompleted:
        // The callback has already been called and so the promise has been
        // resolved or rejected, and the wait has been removed from the
        // dispatcher, so there's nothing to do.
        break;
    }
  }

  bool BeginWait() {
    DCHECK_EQ(wait_state_, WaitState::kCreated);
    zx_status_t status = async_begin_wait(async_get_default_dispatcher(), this);
    if (status == ZX_OK) {
      wait_state_ = WaitState::kStarted;
    } else {
      failed_start_status_ = status;
    }
    return status == ZX_OK;
  }

 private:
  static void StaticOnSignaled(async_dispatcher_t* dispatcher,
                               async_wait_t* wait,
                               zx_status_t status,
                               const zx_packet_signal_t* signal) {
    auto* self = static_cast<WaitPromiseImpl*>(wait);
    self->OnSignaled(status, signal);
  }

  void OnSignaled(zx_status_t status, const zx_packet_signal_t* signal) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    DCHECK_EQ(wait_state_, WaitState::kStarted);
    DCHECK_NE(status, ZX_ERR_CANCELED)
        << "wait should have been canceled before shutdown";

    wait_state_ = WaitState::kCompleted;

    if (status == ZX_OK &&
        (signal->observed & signal->trigger) == signal->trigger) {
      ResolvePromise(signal->observed);
    } else {
      RejectPromise(status, signal->observed);
    }

    GetWaitsForIsolate(isolate_).erase(this);
    // |this| has been deleted.
  }

  void ResolvePromise(zx_signals_t observed) {
    v8::Local<v8::Promise::Resolver> resolver(resolver_.Get(isolate_));
    v8::Local<v8::Context> context(context_.Get(isolate_));
    v8::Local<v8::Object> value = gin::DataObjectBuilder(isolate_)
                                      .Set("status", ZX_OK)
                                      .Set("observed", observed)
                                      .Build();
    resolver->Resolve(context, value).ToChecked();
  }

  void RejectPromise(zx_status_t status, zx_signals_t observed) {
    v8::Local<v8::Promise::Resolver> resolver(resolver_.Get(isolate_));
    v8::Local<v8::Context> context(context_.Get(isolate_));
    v8::Local<v8::Object> value = gin::DataObjectBuilder(isolate_)
                                      .Set("status", status)
                                      .Set("observed", observed)
                                      .Build();
    resolver->Reject(context, value).ToChecked();
  }

  v8::Isolate* isolate_;
  v8::Global<v8::Context> context_;
  v8::Global<v8::Promise::Resolver> resolver_;
  enum class WaitState {
    kCreated,
    kStarted,
    kCompleted,
  } wait_state_;
  zx_status_t failed_start_status_;

  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(WaitPromiseImpl);
};

}  // namespace fidljs

namespace {

v8::Local<v8::Promise> ZxObjectWaitOne(gin::Arguments* args) {
  zx_handle_t handle;
  if (!args->GetNext(&handle)) {
    args->ThrowError();
    return v8::Local<v8::Promise>();
  }

  zx_signals_t signals;
  if (!args->GetNext(&signals)) {
    args->ThrowError();
    return v8::Local<v8::Promise>();
  }

  v8::MaybeLocal<v8::Promise::Resolver> maybe_resolver =
      v8::Promise::Resolver::New(args->GetHolderCreationContext());
  v8::Local<v8::Promise::Resolver> resolver;
  if (maybe_resolver.ToLocal(&resolver)) {
    auto wait = std::make_unique<fidljs::WaitPromiseImpl>(
        args->isolate(), args->GetHolderCreationContext(), resolver, handle,
        signals);
    if (wait->BeginWait()) {
      // The wait will always be notified asynchronously, so it's OK to delay
      // the add until after it has completed successfully. Move |wait| into the
      // set of active waits.
      GetWaitsForIsolate(args->isolate()).insert(std::move(wait));
    }

    // If BeginWait() fails, then |wait| will be deleted here, causing the
    // returned promise to be rejected.
    return resolver->GetPromise();
  }

  return v8::Local<v8::Promise>();
}

v8::Local<v8::Value> ZxChannelCreate(gin::Arguments* args) {
  zx_handle_t channel0, channel1;
  zx_status_t status = zx_channel_create(0, &channel0, &channel1);
  if (status != ZX_OK) {
    return gin::DataObjectBuilder(args->isolate())
        .Set("status", status)
        .Build();
  }

  return gin::DataObjectBuilder(args->isolate())
      .Set("status", status)
      .Set("first", channel0)
      .Set("second", channel1)
      .Build();
}

zx_status_t ZxChannelWrite(gin::Arguments* args) {
  zx_handle_t handle;
  if (!args->GetNext(&handle)) {
    args->ThrowError();
    return ZX_ERR_INVALID_ARGS;
  }

  gin::ArrayBufferView data;
  if (!args->GetNext(&data)) {
    args->ThrowError();
    return ZX_ERR_INVALID_ARGS;
  }

  std::vector<zx_handle_t> handles;
  if (!args->GetNext(&handles)) {
    args->ThrowError();
    return ZX_ERR_INVALID_ARGS;
  }

  zx_status_t status =
      zx_channel_write(handle, 0, data.bytes(), data.num_bytes(),
                       handles.data(), handles.size());
  return status;
}

v8::Local<v8::Object> ZxChannelRead(gin::Arguments* args) {
  zx_handle_t handle;
  if (!args->GetNext(&handle)) {
    args->ThrowError();
    return gin::DataObjectBuilder(args->isolate())
        .Set("status", ZX_ERR_INVALID_ARGS)
        .Build();
  }
  zx::unowned_channel ch(handle);

  uint32_t data_size;
  uint32_t num_handles;
  zx_status_t status =
      ch->read(0, nullptr, nullptr, 0, 0, &data_size, &num_handles);
  DCHECK_EQ(status, ZX_ERR_BUFFER_TOO_SMALL);

  std::vector<zx_handle_t> handles;
  handles.resize(num_handles);

  v8::Local<v8::ArrayBuffer> buf =
      v8::ArrayBuffer::New(args->isolate(), data_size);
  uint32_t actual_bytes, actual_handles;
  status = ch->read(0, buf->GetContents().Data(), handles.data(), data_size,
                    handles.size(), &actual_bytes, &actual_handles);
  DCHECK_EQ(actual_bytes, data_size);
  DCHECK_EQ(actual_handles, num_handles);

  if (status != ZX_OK) {
    return gin::DataObjectBuilder(args->isolate())
        .Set("status", status)
        .Build();
  }

  return gin::DataObjectBuilder(args->isolate())
      .Set("status", status)
      .Set("data", buf)
      .Set("handles", handles)
      .Build();
}

v8::Local<v8::Value> StrToUtf8Array(gin::Arguments* args) {
  std::string str;
  // This converts the string to utf8 from ucs2, so then just repackage the
  // string as an array and return it.
  if (!args->GetNext(&str)) {
    args->ThrowError();
    return v8::Local<v8::Object>();
  }

  // TODO(crbug.com/883496): Not sure how to make a Uint8Array to return here
  // which would be a bit more efficient.
  std::vector<int> data;
  std::copy(str.begin(), str.end(), std::back_inserter(data));
  return gin::ConvertToV8(args->isolate(), data);
}

v8::Local<v8::Value> Utf8ArrayToStr(gin::Arguments* args) {
  gin::ArrayBufferView data;
  if (!args->GetNext(&data)) {
    args->ThrowError();
    return v8::Local<v8::Value>();
  }

  // Get the UTF-8 out into a string, and then rely on ConvertToV8 to convert
  // that to a UCS-2 string.
  return gin::StringToV8(
      args->isolate(), base::StringPiece(static_cast<const char*>(data.bytes()),
                                         data.num_bytes()));
}

}  // namespace

namespace fidljs {

ZxBindings::ZxBindings(v8::Isolate* isolate, v8::Local<v8::Object> global)
    : isolate_(isolate), wait_set_(std::make_unique<WaitSet>()) {
  DCHECK_EQ(isolate->GetData(gin::kEmbedderFuchsia), nullptr);
  isolate->SetData(gin::kEmbedderFuchsia, wait_set_.get());
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

#define SET_CONSTANT(k)                                    \
  global                                                   \
      ->Set(context, gin::StringToSymbol(isolate, "$" #k), \
            gin::ConvertToV8(isolate, k))                  \
      .Check()

  // zx_status_t.
  SET_CONSTANT(ZX_OK);
  SET_CONSTANT(ZX_ERR_INTERNAL);
  SET_CONSTANT(ZX_ERR_NOT_SUPPORTED);
  SET_CONSTANT(ZX_ERR_NO_RESOURCES);
  SET_CONSTANT(ZX_ERR_NO_MEMORY);
  SET_CONSTANT(ZX_ERR_INTERNAL_INTR_RETRY);
  SET_CONSTANT(ZX_ERR_INVALID_ARGS);
  SET_CONSTANT(ZX_ERR_BAD_HANDLE);
  SET_CONSTANT(ZX_ERR_WRONG_TYPE);
  SET_CONSTANT(ZX_ERR_BAD_SYSCALL);
  SET_CONSTANT(ZX_ERR_OUT_OF_RANGE);
  SET_CONSTANT(ZX_ERR_BUFFER_TOO_SMALL);
  SET_CONSTANT(ZX_ERR_BAD_STATE);
  SET_CONSTANT(ZX_ERR_TIMED_OUT);
  SET_CONSTANT(ZX_ERR_SHOULD_WAIT);
  SET_CONSTANT(ZX_ERR_CANCELED);
  SET_CONSTANT(ZX_ERR_PEER_CLOSED);
  SET_CONSTANT(ZX_ERR_NOT_FOUND);
  SET_CONSTANT(ZX_ERR_ALREADY_EXISTS);
  SET_CONSTANT(ZX_ERR_ALREADY_BOUND);
  SET_CONSTANT(ZX_ERR_UNAVAILABLE);
  SET_CONSTANT(ZX_ERR_ACCESS_DENIED);
  SET_CONSTANT(ZX_ERR_IO);
  SET_CONSTANT(ZX_ERR_IO_REFUSED);
  SET_CONSTANT(ZX_ERR_IO_DATA_INTEGRITY);
  SET_CONSTANT(ZX_ERR_IO_DATA_LOSS);
  SET_CONSTANT(ZX_ERR_IO_NOT_PRESENT);
  SET_CONSTANT(ZX_ERR_IO_OVERRUN);
  SET_CONSTANT(ZX_ERR_IO_MISSED_DEADLINE);
  SET_CONSTANT(ZX_ERR_IO_INVALID);
  SET_CONSTANT(ZX_ERR_BAD_PATH);
  SET_CONSTANT(ZX_ERR_NOT_DIR);
  SET_CONSTANT(ZX_ERR_NOT_FILE);
  SET_CONSTANT(ZX_ERR_FILE_BIG);
  SET_CONSTANT(ZX_ERR_NO_SPACE);
  SET_CONSTANT(ZX_ERR_NOT_EMPTY);
  SET_CONSTANT(ZX_ERR_STOP);
  SET_CONSTANT(ZX_ERR_NEXT);
  SET_CONSTANT(ZX_ERR_ASYNC);
  SET_CONSTANT(ZX_ERR_PROTOCOL_NOT_SUPPORTED);
  SET_CONSTANT(ZX_ERR_ADDRESS_UNREACHABLE);
  SET_CONSTANT(ZX_ERR_ADDRESS_IN_USE);
  SET_CONSTANT(ZX_ERR_NOT_CONNECTED);
  SET_CONSTANT(ZX_ERR_CONNECTION_REFUSED);
  SET_CONSTANT(ZX_ERR_CONNECTION_RESET);
  SET_CONSTANT(ZX_ERR_CONNECTION_ABORTED);

  // Handle APIs.
  global
      ->Set(context, gin::StringToSymbol(isolate, "$ZxObjectWaitOne"),
            gin::CreateFunctionTemplate(isolate,
                                        base::BindRepeating(ZxObjectWaitOne))
                ->GetFunction(context)
                .ToLocalChecked())
      .Check();
  global
      ->Set(context, gin::StringToSymbol(isolate, "$zx_handle_close"),
            gin::CreateFunctionTemplate(isolate,
                                        base::BindRepeating(zx_handle_close))
                ->GetFunction(context)
                .ToLocalChecked())
      .Check();
  SET_CONSTANT(ZX_HANDLE_INVALID);
  SET_CONSTANT(ZX_TIME_INFINITE);

  // Channel APIs.
  global
      ->Set(context, gin::StringToSymbol(isolate, "$ZxChannelCreate"),
            gin::CreateFunctionTemplate(isolate,
                                        base::BindRepeating(&ZxChannelCreate))
                ->GetFunction(context)
                .ToLocalChecked())
      .Check();
  global
      ->Set(context, gin::StringToSymbol(isolate, "$ZxChannelWrite"),
            gin::CreateFunctionTemplate(isolate,
                                        base::BindRepeating(&ZxChannelWrite))
                ->GetFunction(context)
                .ToLocalChecked())
      .Check();
  global
      ->Set(context, gin::StringToSymbol(isolate, "$ZxChannelRead"),
            gin::CreateFunctionTemplate(isolate,
                                        base::BindRepeating(&ZxChannelRead))
                ->GetFunction(context)
                .ToLocalChecked())
      .Check();
  SET_CONSTANT(ZX_CHANNEL_READABLE);
  SET_CONSTANT(ZX_CHANNEL_WRITABLE);
  SET_CONSTANT(ZX_CHANNEL_PEER_CLOSED);
  SET_CONSTANT(ZX_CHANNEL_READ_MAY_DISCARD);
  SET_CONSTANT(ZX_CHANNEL_MAX_MSG_BYTES);
  SET_CONSTANT(ZX_CHANNEL_MAX_MSG_HANDLES);

  // Utilities to make string handling easier to convert to/from UCS-2 (JS) <->
  // UTF-8 (FIDL).
  global
      ->Set(context, gin::StringToSymbol(isolate, "$FidlJsStrToUtf8Array"),
            gin::CreateFunctionTemplate(isolate,
                                        base::BindRepeating(&StrToUtf8Array))
                ->GetFunction(context)
                .ToLocalChecked())
      .Check();
  global
      ->Set(context, gin::StringToSymbol(isolate, "$FidlJsUtf8ArrayToStr"),
            gin::CreateFunctionTemplate(isolate,
                                        base::BindRepeating(&Utf8ArrayToStr))
                ->GetFunction(context)
                .ToLocalChecked())
      .Check();

#undef SET_CONSTANT
}

ZxBindings::~ZxBindings() {
  wait_set_->clear();
  isolate_->SetData(gin::kEmbedderFuchsia, nullptr);
}

}  // namespace fidljs
