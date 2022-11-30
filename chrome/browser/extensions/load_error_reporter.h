// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_LOAD_ERROR_REPORTER_H_
#define CHROME_BROWSER_EXTENSIONS_LOAD_ERROR_REPORTER_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/task/single_thread_task_runner.h"

namespace base {
class FilePath;
}

namespace content {
class BrowserContext;
}

namespace extensions {

// Exposes an easy way for the various components of the extension system to
// report load errors. This is a singleton that lives on the UI thread, with the
// exception of ReportError() which may be called from any thread.
// TODO(aa): Add ReportError(extension_id, message, be_noisy), so that we can
// report errors that are specific to a particular extension.
class LoadErrorReporter {
 public:
  class Observer {
   public:
    virtual ~Observer() {}

    // Called when an unpacked extension fails to load.
    virtual void OnLoadFailure(content::BrowserContext* browser_context,
                               const base::FilePath& extension_path,
                               const std::string& error) = 0;
  };

  LoadErrorReporter(const LoadErrorReporter&) = delete;
  LoadErrorReporter& operator=(const LoadErrorReporter&) = delete;

  // Initializes the error reporter. Must be called before any other methods
  // and on the UI thread.
  static void Init(bool enable_noisy_errors);

  // Get the singleton instance.
  static LoadErrorReporter* GetInstance();

  // Report an extension load error. This forwards to ReportError() after
  // sending an EXTENSION_LOAD_ERROR notification.
  // TODO(rdevlin.cronin): There's a lot wrong with this. But some of our
  // systems rely on the notification. Investigate what it will take to remove
  // the notification and this method.
  void ReportLoadError(const base::FilePath& extension_path,
                       const std::string& error,
                       content::BrowserContext* browser_context,
                       bool be_noisy);

  // Report an error. Errors always go to VLOG(1). Optionally, they can also
  // cause a noisy alert box.
  void ReportError(const std::u16string& message, bool be_noisy);

  // Get the errors that have been reported so far.
  const std::vector<std::u16string>* GetErrors();

  // Clear the list of errors reported so far.
  void ClearErrors();

  void AddObserver(Observer* observer);

  void RemoveObserver(Observer* observer);

 private:
  static LoadErrorReporter* instance_;

  explicit LoadErrorReporter(bool enable_noisy_errors);
  ~LoadErrorReporter();

  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;
  std::vector<std::u16string> errors_;
  bool enable_noisy_errors_;

  base::ObserverList<Observer>::Unchecked observers_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_LOAD_ERROR_REPORTER_H_
