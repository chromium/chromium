// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINTER_QUERY_H_
#define CHROME_BROWSER_PRINTING_PRINTER_QUERY_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/values.h"
#include "printing/print_job_constants.h"
#include "printing/print_settings.h"
#include "printing/printing_context.h"

namespace base {
class Location;
}

namespace printing {

class PrintJobWorker;

// Query the printer for settings.
class PrinterQuery {
 public:
  // GetSettings() UI parameter.
  enum class GetSettingsAskParam {
    DEFAULTS,
    ASK_USER,
  };

  // Can only be called on the IO thread.
  PrinterQuery(int render_process_id, int render_frame_id);
  virtual ~PrinterQuery();

  // Detach the PrintJobWorker associated to this object. Virtual so that tests
  // can override.
  // Called on the UI thread.
  // TODO(thestig): Do |worker_| and |callback_| need locks?
  virtual std::unique_ptr<PrintJobWorker> DetachWorker();

  const PrintSettings& settings() const;

  std::unique_ptr<PrintSettings> ExtractSettings();

  // Initializes the printing context. It is fine to call this function multiple
  // times to reinitialize the settings. |web_contents_observer| can be queried
  // to find the owner of the print setting dialog box. It is unused when
  // |ask_for_user_settings| is DEFAULTS.
  // Caller has to ensure that |this| is alive until |callback| is run.
  void GetSettings(GetSettingsAskParam ask_user_for_settings,
                   int expected_page_count,
                   bool has_selection,
                   MarginType margin_type,
                   bool is_scripted,
                   bool is_modifiable,
                   base::OnceClosure callback);

  // Updates the current settings with |new_settings| dictionary values.
  // Caller has to ensure that |this| is alive until |callback| is run.
  virtual void SetSettings(base::Value new_settings,
                           base::OnceClosure callback);

#if defined(OS_CHROMEOS)
  // Updates the current settings with |new_settings|.
  // Caller has to ensure that |this| is alive until |callback| is run.
  void SetSettingsFromPOD(std::unique_ptr<PrintSettings> new_settings,
                          base::OnceClosure callback);
#endif

  // Stops the worker thread since the client is done with this object.
  virtual void StopWorker();

  int cookie() const;
  PrintingContext::Result last_status() const { return last_status_; }

  // Returns if a worker thread is still associated to this instance.
  bool is_valid() const;

  // Posts the given task to be run.
  bool PostTask(const base::Location& from_here, base::OnceClosure task);

 protected:
  // Virtual so that tests can override.
  virtual void GetSettingsDone(base::OnceClosure callback,
                               std::unique_ptr<PrintSettings> new_settings,
                               PrintingContext::Result result);

  void PostSettingsDoneToIO(base::OnceClosure callback,
                            std::unique_ptr<PrintSettings> new_settings,
                            PrintingContext::Result result);

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
  PrintingContext::Result last_status_ = PrintingContext::FAILED;

  // All the UI is done in a worker thread because many Win32 print functions
  // are blocking and enters a message loop without your consent. There is one
  // worker thread per print job.
  std::unique_ptr<PrintJobWorker> worker_;

  DISALLOW_COPY_AND_ASSIGN(PrinterQuery);
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_PRINTER_QUERY_H_
