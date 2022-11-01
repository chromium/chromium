// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/dbus/profiler_status_service_provider.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "components/metrics/call_stack_profile_metrics_provider.h"
#include "dbus/message.h"

namespace ash {

ProfilerStatusServiceProvider::ProfilerStatusServiceProvider() = default;

ProfilerStatusServiceProvider::~ProfilerStatusServiceProvider() = default;

void ProfilerStatusServiceProvider::Start(
    scoped_refptr<dbus::ExportedObject> exported_object) {
  exported_object->ExportMethod(
      kServiceName, kGetSuccessfullyCollectedCountsMethod,
      base::BindRepeating(
          &ProfilerStatusServiceProvider::GetSuccessfullyCollectedCounts),
      base::BindOnce(&ProfilerStatusServiceProvider::OnExported));
}

// static
void ProfilerStatusServiceProvider::OnExported(
    const std::string& interface_name,
    const std::string& method_name,
    bool success) {
  if (!success) {
    LOG(ERROR) << "Failed to export " << interface_name << "." << method_name;
  }
}

// static
void ProfilerStatusServiceProvider::GetSuccessfullyCollectedCounts(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  metrics::CallStackProfileMetricsProvider::ProcessThreadCount counts =
      metrics::CallStackProfileMetricsProvider::
          GetSuccessfullyCollectedCounts();

  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  dbus::MessageWriter writer(response.get());

  dbus::MessageWriter array_writer(nullptr);
  // Send an array of structures, where each structure contains three ints: the
  // process type, the thread type, and the # of successfully-stack-walked
  // profile samples received.
  writer.OpenArray("(iii)", &array_writer);
  for (const auto& [process, thread_counts] : counts) {
    for (auto [thread, count] : thread_counts) {
      dbus::MessageWriter struct_writer(nullptr);
      array_writer.OpenStruct(&struct_writer);
      struct_writer.AppendInt32(process);
      struct_writer.AppendInt32(thread);
      struct_writer.AppendInt32(count);
      array_writer.CloseContainer(&struct_writer);
    }
  }
  writer.CloseContainer(&array_writer);

  std::move(response_sender).Run(std::move(response));
}

}  // namespace ash
