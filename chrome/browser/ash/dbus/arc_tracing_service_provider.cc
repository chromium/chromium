// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/dbus/arc_tracing_service_provider.h"

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/arc/tracing/arc_tracing_graphics_model.h"
#include "chrome/browser/ash/arc/tracing/overview_tracing_handler.h"
#include "chrome/browser/browser_process.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/aura/window.h"

namespace ash {
namespace {
constexpr int kMaxStatusMessagesCount = 20;
constexpr char kTraceStartedMsg[] = "Trace started";
}  // namespace

ArcTracingServiceProvider::ArcTracingServiceProvider() = default;

ArcTracingServiceProvider::~ArcTracingServiceProvider() = default;

void ArcTracingServiceProvider::Start(
    scoped_refptr<dbus::ExportedObject> exported_object) {
  exported_object->ExportMethod(
      arc::tracing::kArcTracingInterfaceName,
      arc::tracing::kArcTracingStartMethod,
      base::BindRepeating(&ArcTracingServiceProvider::StartTrace,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&ArcTracingServiceProvider::OnExported,
                     weak_ptr_factory_.GetWeakPtr()));
  exported_object->ExportMethod(
      arc::tracing::kArcTracingInterfaceName,
      arc::tracing::kArcTracingGetStatusMethod,
      base::BindRepeating(&ArcTracingServiceProvider::GetStatus,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&ArcTracingServiceProvider::OnExported,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ArcTracingServiceProvider::AddStatusMessage(std::string_view status) {
  msgs_.emplace_back(status);
  if (msgs_.size() > kMaxStatusMessagesCount) {
    msgs_.pop_front();
  }
}

void ArcTracingServiceProvider::OnExported(const std::string& interface_name,
                                           const std::string& method_name,
                                           bool success) {
  LOG_IF(ERROR, !success) << "Failed to export " << interface_name << "."
                          << method_name;
}

void ArcTracingServiceProvider::OnTraceEnd(
    std::unique_ptr<arc::OverviewTracingResult> result) {
  if (result->path.empty()) {
    AddStatusMessage(result->status);
  } else if (auto* information =
                 result->model.GetDict().FindDict(arc::kKeyInformation);
             information) {
    auto pfps = information->FindDouble(arc::kKeyPerceivedFps);
    auto duration = information->FindDouble(arc::kKeyDuration);
    AddStatusMessage(base::StringPrintf(
        "%s: %s - perceived FPS=%.2f, duration=%.2fs", result->status.c_str(),
        result->path.value().c_str(), pfps.value_or(0),
        duration.value_or(0) / 1'000'000.0));
  }
  // Do this in a separate task because the handler may still have code to run
  // after we return.
  base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(
      FROM_HERE, std::move(handler_));
}

std::unique_ptr<arc::OverviewTracingHandler>
ArcTracingServiceProvider::NewHandler() {
  return std::make_unique<arc::OverviewTracingHandler>(
      arc::OverviewTracingHandler::ArcWindowFocusChangeCb());
}

void ArcTracingServiceProvider::StartTrace(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  if (handler_) {
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(method_call, DBUS_ERROR_FAILED,
                                                 "Trace already in progress"));
    return;
  }

  dbus::MessageReader reader(method_call);

  double max_trace_seconds;
  if (!reader.PopDouble(&max_trace_seconds)) {
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_INVALID_ARGS,
            "Expect max trace time as type double in seconds"));
    return;
  }
  auto handler = NewHandler();

  auto max_trace_time = base::Seconds(max_trace_seconds);
  if (max_trace_time < base::Seconds(1)) {
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_INVALID_ARGS,
            "Max trace seconds out of range; must be >= 1"));
    return;
  }

  if (!handler->arc_window_is_active()) {
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(method_call, DBUS_ERROR_FAILED,
                                                 "ARC window isn't active"));
    return;
  }

  auto extra_windows = handler->NonTraceTargetWindows();
  if (!extra_windows.empty()) {
    std::vector<std::string> extra_win_msg = {
        "Extra windows are open. Close them and try the trace again: "};
    std::string_view delim = "";
    for (auto window : extra_windows) {
      extra_win_msg.emplace_back(delim);
      extra_win_msg.emplace_back("|");
      extra_win_msg.emplace_back(base::UTF16ToUTF8(window->GetTitle()));
      extra_win_msg.emplace_back("|");
      delim = ", ";
    }
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(method_call, DBUS_ERROR_FAILED,
                                                 base::StrCat(extra_win_msg)));
    return;
  }

  handler_ = std::move(handler);
  handler_->set_graphics_model_ready_cb(base::BindRepeating(
      &ArcTracingServiceProvider::OnTraceEnd, weak_ptr_factory_.GetWeakPtr()));
  handler_->set_start_build_model_cb(
      base::BindRepeating(&ArcTracingServiceProvider::AddStatusMessage,
                          weak_ptr_factory_.GetWeakPtr(), "Building model..."));
  handler_->StartTracing(trace_outdir_, max_trace_time);

  auto response = dbus::Response::FromMethodCall(method_call);
  dbus::MessageWriter writer(response.get());
  writer.AppendString(kTraceStartedMsg);
  AddStatusMessage(kTraceStartedMsg);
  std::move(response_sender).Run(std::move(response));
}

void ArcTracingServiceProvider::GetStatus(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  auto response = dbus::Response::FromMethodCall(method_call);
  dbus::MessageWriter writer(response.get());
  for (const auto& msg : msgs_) {
    writer.AppendString(msg);
  }
  std::move(response_sender).Run(std::move(response));
}

}  // namespace ash
