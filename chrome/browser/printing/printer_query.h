// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINTER_QUERY_H_
#define CHROME_BROWSER_PRINTING_PRINTER_QUERY_H_

#include <memory>

#include "base/callback.h"
#include "base/values.h"
#include "build/build_config.h"
#include "printing/mojom/print.mojom.h"
#include "printing/print_settings.h"
#include "printing/printing_context.h"

namespace base {
class Location;
}

namespace content {
struct GlobalRenderFrameHostId;
}

namespace printing {

class PrintJobWorker;

using CreatePrintJobWorkerCallback =
    base::RepeatingCallback<std::unique_ptr<PrintJobWorker>(
        content::GlobalRenderFrameHostId rfh_id)>;

// Query the printer for settings.  All code in this class runs in the UI
// thread.
class PrinterQuery {
 public:
  explicit PrinterQuery(content::GlobalRenderFrameHostId rfh_id);

  PrinterQuery(const PrinterQuery&) = delete;
  PrinterQuery& operator=(const PrinterQuery&) = delete;

  virtual ~PrinterQuery();

  // Detach the PrintJobWorker associated to this object. Virtual so that tests
  // can override.
  virtual std::unique_ptr<PrintJobWorker> DetachWorker();

  const PrintSettings& settings() const;

  std::unique_ptr<PrintSettings> ExtractSettings();

  // Initializes the printing context. It is fine to call this function multiple
  // times to reinitialize the settings.
  // Caller has to ensure that `this` is alive until `callback` is run.
  void GetDefaultSettings(base::OnceClosure callback,
                          bool is_modifiable,
                          bool want_pdf_settings);
  void GetSettingsFromUser(uint32_t expected_page_count,
                           bool has_selection,
                           mojom::MarginType margin_type,
                           bool is_scripted,
                           bool is_modifiable,
                           base::OnceClosure callback);

  // Updates the current settings with `new_settings` dictionary values.
  // Caller has to ensure that `this` is alive until `callback` is run.
  virtual void SetSettings(base::Value::Dict new_settings,
                           base::OnceClosure callback);

#if BUILDFLAG(IS_CHROMEOS)
  // Updates the current settings with `new_settings`.
  // Caller has to ensure that `this` is alive until `callback` is run.
  void SetSettingsFromPOD(std::unique_ptr<PrintSettings> new_settings,
                          base::OnceClosure callback);
#endif

  // Stops the worker thread since the client is done with this object.
  virtual void StopWorker();

  int cookie() const;
  mojom::ResultCode last_status() const { return last_status_; }

  // Returns if a worker thread is still associated to this instance.
  bool is_valid() const;

  // Posts the given task to be run.
  bool PostTask(const base::Location& from_here, base::OnceClosure task);

  // Provide an override for generating worker threads in tests.
  static void SetCreatePrintJobWorkerCallbackForTest(
      CreatePrintJobWorkerCallback* callback);

 protected:
  // Virtual so that tests can override.
  virtual void GetSettingsDone(base::OnceClosure callback,
                               absl::optional<bool> maybe_is_modifiable,
                               std::unique_ptr<PrintSettings> new_settings,
                               mojom::ResultCode result);

  void PostSettingsDone(base::OnceClosure callback,
                        absl::optional<bool> maybe_is_modifiable,
                        std::unique_ptr<PrintSettings> new_settings,
                        mojom::ResultCode result);

  void SetSettingsForTest(std::unique_ptr<PrintSettings> settings);

 private:
  // Lazy create the worker thread. There is one worker thread per print job.
  void StartWorker();

  // Cache of the print context settings for access in the UI thread.
  std::unique_ptr<PrintSettings> settings_;

  // Is the Print... dialog box currently shown.
  bool is_print_dialog_box_shown_ = false;

  // Cookie that make this instance unique.
  int cookie_;

  // Results from the last GetSettingsDone() callback.
  mojom::ResultCode last_status_ = mojom::ResultCode::kFailed;

  // All the UI is done in a worker thread because many Win32 print functions
  // are blocking and enters a message loop without your consent. There is one
  // worker thread per print job.
  std::unique_ptr<PrintJobWorker> worker_;
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_PRINTER_QUERY_H_
