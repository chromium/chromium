// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/launcher_search_provider/error_reporter.h"

#include "base/memory/ptr_util.h"
#include "content/public/browser/render_frame_host.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"

namespace chromeos {
namespace launcher_search_provider {

ErrorReporter::ErrorReporter(content::RenderFrameHost* host) : host_(host) {
}

ErrorReporter::~ErrorReporter() {
}

void ErrorReporter::Warn(const std::string& message) {
  DCHECK(host_);

  host_->AddMessageToConsole(blink::mojom::ConsoleMessageLevel::kWarning,
                             message);
}

std::unique_ptr<ErrorReporter> ErrorReporter::Duplicate() {
  return base::WrapUnique(new ErrorReporter(host_));
}

}  // namespace launcher_search_provider
}  // namespace chromeos
