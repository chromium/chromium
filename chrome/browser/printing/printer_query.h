// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINTER_QUERY_H_
#define CHROME_BROWSER_PRINTING_PRINTER_QUERY_H_

#include <memory>
#include <optional>

#include "base/functional/callback.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/public/browser/global_routing_id.h"
#include "printing/buildflags/buildflags.h"
#include "printing/mojom/print.mojom.h"
#include "printing/print_settings.h"
#include "printing/printing_context.h"

#if BUILDFLAG(ENABLE_OOP_PRINTING)
#include "chrome/browser/printing/print_backend_service_manager.h"
#endif

namespace content {
class WebContents;
}

namespace printing {

class PrintJob;
class PrintJobWorker;
class PrinterQuery;

using CreatePrinterQueryCallback =
    base::RepeatingCallback<std::unique_ptr<PrinterQuery>(
        content::GlobalRenderFrameHostId rfh_id)>;

// Query the printer for settings. It initializes the PrintingContext, which can
// be blocking and/or run a message loop, and eventually is transferred to a new
// PrintJobWorker. All code in this class runs in the UI thread.
class PrinterQuery {
 public:
  using SettingsCallback =
      base::OnceCallback<void(std::unique_ptr<PrintSettings>,
                              mojom::ResultCode)>;

#if BUILDFLAG(IS_WIN)
  using OnDidUpdatePrintableAreaCallback =
      base::OnceCallback<void(bool success)>;
#endif

  static std::unique_ptr<PrinterQuery> Create(
      content::GlobalRenderFrameHostId rfh_id);

  PrinterQuery(const PrinterQuery&) = delete;
  PrinterQuery& operator=(const PrinterQuery&) = delete;

  virtual ~PrinterQuery();

  // Creates a PrintJobWorker from this object's PrintingContext, transferring
  // ownership. This instance becomes invalid after calling this function.
  virtual std::unique_ptr<PrintJobWorker> TransferContextToNewWorker(
      PrintJob* print_job);

  const PrintSettings& settings() const;

  content::GlobalRenderFrameHostId rfh_id() const { return rfh_id_; }

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

#if BUILDFLAG(IS_WIN)
  // Updates the printable area of the provided `PrintSettings` object.
  // Caller has to ensure that `this` and `print_settings` are alive until
  // `callback` runs.
  // TODO(crbug.com/40260379):  Remove this if the printable areas can be made
  // fully available from `PrintBackend::GetPrinterSemanticCapsAndDefaults()`.
  virtual void UpdatePrintableArea(PrintSettings* print_settings,
                                   OnDidUpdatePrintableAreaCallback callback);
#endif

  // Sets the printable area in `print_settings` to be the default printable
  // area. Intended to be used only for virtual printers. Does not communicate
  // with printer drivers, so it does not require special OOPPD handling.
  static void ApplyDefaultPrintableAreaToVirtualPrinterPrintSettings(
      PrintSettings& print_settings);

#if BUILDFLAG(ENABLE_OOP_PRINTING)
  // Provide the client ID when the caller has registered with the
  // `PrintBackendServiceManager` for getting settings for system print.
  // Only intended to be used when out-of-process printing is in use, will
  // DCHECK if used for in-browser printing.
  virtual void SetClientId(PrintBackendServiceManager::ClientId client_id);
#endif

  int cookie() const;
  mojom::ResultCode last_status() const { return last_status_; }

  // Returns true if a PrintingContext is still associated to this instance.
  bool is_valid() const;

  // Provide an override for generating worker threads in tests.
  static void SetCreatePrinterQueryCallbackForTest(
      CreatePrinterQueryCallback* callback);

 protected:
  explicit PrinterQuery(content::GlobalRenderFrameHostId rfh_id);

  // Returns the WebContents this work corresponds to.
  content::WebContents* GetWebContents();

  // Reports settings back to `callback`.
  void InvokeSettingsCallback(SettingsCallback callback,
                              mojom::ResultCode result);

  // Virtual so that tests can override.
  virtual void GetSettingsDone(base::OnceClosure callback,
                               std::optional<bool> maybe_is_modifiable,
                               std::unique_ptr<PrintSettings> new_settings,
                               mojom::ResultCode result);

  void PostSettingsDone(base::OnceClosure callback,
                        std::optional<bool> maybe_is_modifiable,
                        std::unique_ptr<PrintSettings> new_settings,
                        mojom::ResultCode result);

  void SetSettingsForTest(std::unique_ptr<PrintSettings> settings);

  // Asks the user for print settings.
  // Required on Mac and Linux. Windows can display UI from non-main threads,
  // but sticks with this for consistency.
  virtual void GetSettingsWithUI(uint32_t document_page_count,
                                 bool has_selection,
                                 bool is_scripted,
                                 SettingsCallback callback);

  // Initializes the print settings for PDF.
  std::unique_ptr<PrintSettings> GetPdfSettings();

  // Use the default settings. When using GTK+ or Mac, this can still end up
  // displaying a dialog. So this needs to happen from the UI thread on these
  // systems.
  virtual void UseDefaultSettings(SettingsCallback callback);

  // Called to update the print settings.
  virtual void UpdatePrintSettings(base::Value::Dict new_settings,
                                   SettingsCallback callback);

#if BUILDFLAG(IS_CHROMEOS)
  // Called to update the print settings.
  void UpdatePrintSettingsFromPOD(
      std::unique_ptr<printing::PrintSettings> new_settings,
      SettingsCallback callback);
#endif

  // Used by `TransferContextToNewWorker()`.  Virtual to support testing.
  virtual std::unique_ptr<PrintJobWorker> CreatePrintJobWorker(
      PrintJob* print_job);

  PrintingContext* printing_context() { return printing_context_.get(); }

  // Printing context delegate.
  std::unique_ptr<PrintingContext::Delegate> printing_context_delegate_;

  // Information about the printer setting.
  std::unique_ptr<PrintingContext> printing_context_;

  const content::GlobalRenderFrameHostId rfh_id_;

  // Is the Print... dialog box currently shown.
  bool is_print_dialog_box_shown_ = false;

 private:
  // Cache of the print context settings.
  std::unique_ptr<PrintSettings> settings_;

  // Cookie that make this instance unique.
  int cookie_;

  // Results from the last GetSettingsDone() callback.
  mojom::ResultCode last_status_ = mojom::ResultCode::kFailed;

  base::WeakPtrFactory<PrinterQuery> weak_factory_{this};
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_PRINTER_QUERY_H_
