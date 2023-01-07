// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/load_error_waiter.h"

namespace extensions {

LoadErrorWaiter::LoadErrorWaiter() {
  load_error_observation_.Observe(extensions::LoadErrorReporter::GetInstance());
}

LoadErrorWaiter::~LoadErrorWaiter() = default;

void LoadErrorWaiter::OnLoadFailure(content::BrowserContext* browser_context,
                                    const base::FilePath& file_path,
                                    const std::string& error) {
  load_error_seen_ = true;
  run_loop_.Quit();
}

bool LoadErrorWaiter::Wait() {
  if (!load_error_seen_) {
    run_loop_.Run();
  }
  return load_error_seen_;
}

}  // namespace extensions
