// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_COMMON_BACKEND_WEBUI_SYSLOG_EMITTER_H_
#define ASH_WEBUI_COMMON_BACKEND_WEBUI_SYSLOG_EMITTER_H_

#include "ash/webui/common/mojom/webui_syslog_emitter.mojom.h"
#include "base/values.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace ash {
// A handler for handling SYSLOG requests from a WebUI.
class WebUiSyslogEmitter : public common::mojom::WebUiSyslogEmitter {
 public:
  WebUiSyslogEmitter();

  ~WebUiSyslogEmitter() override;

  void BindInterface(
      mojo::PendingReceiver<common::mojom::WebUiSyslogEmitter> receiver);

  // common::mojom::SyslogHandler
  void EmitSyslog(const std::string& prefix,
                  const std::string& message) override;

 private:
  mojo::Receiver<common::mojom::WebUiSyslogEmitter>
      webui_syslog_emitter_receiver_{this};

  base::WeakPtrFactory<WebUiSyslogEmitter> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_WEBUI_COMMON_BACKEND_WEBUI_SYSLOG_EMITTER_H_
