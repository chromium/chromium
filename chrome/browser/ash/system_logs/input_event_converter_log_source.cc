// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_logs/input_event_converter_log_source.h"

#include "content/public/browser/browser_thread.h"
#include "ui/ozone/public/input_controller.h"
#include "ui/ozone/public/ozone_platform.h"

using content::BrowserThread;

namespace system_logs {

namespace {

void SaveDescription(SysLogsSourceCallback callback,
                     const std::string& description) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto response = std::make_unique<SystemLogsResponse>();

  response->emplace("ozone_evdev_input_event_converters", description);

  std::move(callback).Run(std::move(response));
}

}  // namespace

InputEventConverterLogSource::InputEventConverterLogSource()
    : SystemLogsSource("InputEvdevDevices") {}

InputEventConverterLogSource::~InputEventConverterLogSource() = default;

void InputEventConverterLogSource::Fetch(SysLogsSourceCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());

  ui::InputController* input_controller =
      ui::OzonePlatform::GetInstance()->GetInputController();
  input_controller->DescribeForLog(
      base::BindOnce(&SaveDescription, std::move(callback)));
}

}  // namespace system_logs
