// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/common/backend/webui_syslog_emitter.h"

#include <string>

#include "base/syslog_logging.h"

namespace ash {

WebUiSyslogEmitter::WebUiSyslogEmitter() = default;

WebUiSyslogEmitter::~WebUiSyslogEmitter() = default;

void WebUiSyslogEmitter::BindInterface(
    mojo::PendingReceiver<common::mojom::WebUiSyslogEmitter> receiver) {
  if (webui_syslog_emitter_receiver_.is_bound()) {
    webui_syslog_emitter_receiver_.reset();
  }
  webui_syslog_emitter_receiver_.Bind(std::move(receiver));
}

void WebUiSyslogEmitter::EmitSyslog(const std::string& prefix,
                                    const std::string& message) {
  SYSLOG(INFO) << prefix << " " << message;
}

}  // namespace ash
