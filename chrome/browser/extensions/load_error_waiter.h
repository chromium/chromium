// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_LOAD_ERROR_WAITER_H_
#define CHROME_BROWSER_EXTENSIONS_LOAD_ERROR_WAITER_H_

#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "chrome/browser/extensions/load_error_reporter.h"

namespace extensions {

class LoadErrorWaiter : public extensions::LoadErrorReporter::Observer {
 public:
  LoadErrorWaiter();
  ~LoadErrorWaiter() override;
  LoadErrorWaiter(const LoadErrorWaiter& other) = delete;
  LoadErrorWaiter& operator=(const LoadErrorWaiter& other) = delete;

  // LoadErrorReporter::Observer:
  void OnLoadFailure(content::BrowserContext* browser_context,
                     const base::FilePath& file_path,
                     const std::string& error) override;

  // Waits until the observed LoadErrorReporter report a load error via the
  // OnLoadFailure event.
  bool Wait();

 private:
  base::ScopedObservation<extensions::LoadErrorReporter,
                          extensions::LoadErrorReporter::Observer>
      load_error_observation_{this};

  base::RunLoop run_loop_;
  bool load_error_seen_ = false;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_LOAD_ERROR_WAITER_H_
