// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/printer_query.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/notreached.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/printing/print_job_worker.h"
#include "components/crash/core/common/crash_keys.h"
#include "components/device_event_log/device_event_log.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/web_contents.h"
#include "printing/backend/print_backend.h"
#include "printing/buildflags/buildflags.h"
#include "printing/print_settings.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/android/tab_printer.h"
#include "printing/printing_context_android.h"
#endif

#if BUILDFLAG(ENABLE_OOP_PRINTING)
#include "chrome/browser/printing/oop_features.h"
#include "chrome/browser/printing/printer_query_oop.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/strings/utf_string_conversions.h"
#endif

namespace printing {

namespace {

PrintingContext::ProcessBehavior GetPrintingContextProcessBehavior() {
#if BUILDFLAG(ENABLE_OOP_PRINTING)
  if (ShouldPrintJobOop()) {
    return PrintingContext::ProcessBehavior::kOopEnabledSkipSystemCalls;
  }
#endif
  return PrintingContext::ProcessBehavior::kOopDisabled;
}

class PrintingContextDelegate : public PrintingContext::Delegate {
 public:
  explicit PrintingContextDelegate(content::GlobalRenderFrameHostId rfh_id);

  PrintingContextDelegate(const PrintingContextDelegate&) = delete;
  PrintingContextDelegate& operator=(const PrintingContextDelegate&) = delete;

  ~PrintingContextDelegate() override;

  gfx::NativeView GetParentView() override;
  std::string GetAppLocale() override;

  // Not exposed to PrintingContext::Delegate because of dependency issues.
  content::WebContents* GetWebContents();

  content::GlobalRenderFrameHostId rfh_id() const { return rfh_id_; }

 private:
  const content::GlobalRenderFrameHostId rfh_id_;
};

PrintingContextDelegate::PrintingContextDelegate(
    content::GlobalRenderFrameHostId rfh_id)
    : rfh_id_(rfh_id) {}

PrintingContextDelegate::~PrintingContextDelegate() = default;

gfx::NativeView PrintingContextDelegate::GetParentView() {
  content::WebContents* wc = GetWebContents();
  return wc ? wc->GetNativeView() : nullptr;
}

content::WebContents* PrintingContextDelegate::GetWebContents() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto* rfh = content::RenderFrameHost::FromID(rfh_id_);
  return rfh ? content::WebContents::FromRenderFrameHost(rfh) : nullptr;
}

std::string PrintingContextDelegate::GetAppLocale() {
  return g_browser_process->GetApplicationLocale();
}

CreatePrinterQueryCallback* g_create_printer_query_for_testing = nullptr;

}  // namespace

// static
std::unique_ptr<PrinterQuery> PrinterQuery::Create(
    content::GlobalRenderFrameHostId rfh_id) {
  if (g_create_printer_query_for_testing) {
    return g_create_printer_query_for_testing->Run(rfh_id);
  }

#if BUILDFLAG(ENABLE_OOP_PRINTING)
  if (ShouldPrintJobOop()) {
    return base::WrapUnique(new PrinterQueryOop(rfh_id));
  }
#endif
  return base::WrapUnique(new PrinterQuery(rfh_id));
}

PrinterQuery::PrinterQuery(content::GlobalRenderFrameHostId rfh_id)
    : printing_context_delegate_(
          std::make_unique<PrintingContextDelegate>(rfh_id)),
      printing_context_(
          PrintingContext::Create(printing_context_delegate_.get(),
                                  GetPrintingContextProcessBehavior())),
      rfh_id_(rfh_id),
      cookie_(PrintSettings::NewCookie()) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

PrinterQuery::~PrinterQuery() {
  // The job should be finished (or at least canceled) when it is destroyed.
  DCHECK(!is_print_dialog_box_shown_);
}

void PrinterQuery::GetSettingsDone(base::OnceClosure callback,
                                   std::optional<bool> maybe_is_modifiable,
                                   std::unique_ptr<PrintSettings> new_settings,
                                   mojom::ResultCode result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  is_print_dialog_box_shown_ = false;
  last_status_ = result;
  if (result == mojom::ResultCode::kSuccess) {
    settings_ = std::move(new_settings);
    if (maybe_is_modifiable.has_value())
      settings_->set_is_modifiable(maybe_is_modifiable.value());
    cookie_ = PrintSettings::NewCookie();
  } else {
    // Failure.
    cookie_ = PrintSettings::NewInvalidCookie();
  }

  std::move(callback).Run();
}

void PrinterQuery::PostSettingsDone(base::OnceClosure callback,
                                    std::optional<bool> maybe_is_modifiable,
                                    std::unique_ptr<PrintSettings> new_settings,
                                    mojom::ResultCode result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // `this` is owned by `callback`, so `base::Unretained()` is safe.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&PrinterQuery::GetSettingsDone, base::Unretained(this),
                     std::move(callback), maybe_is_modifiable,
                     std::move(new_settings), result));
}

std::unique_ptr<PrintJobWorker> PrinterQuery::TransferContextToNewWorker(
    PrintJob* print_job) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  return CreatePrintJobWorker(print_job);
}

const PrintSettings& PrinterQuery::settings() const {
  return *settings_;
}

std::unique_ptr<PrintSettings> PrinterQuery::ExtractSettings() {
  return std::move(settings_);
}

void PrinterQuery::SetSettingsForTest(std::unique_ptr<PrintSettings> settings) {
  settings_ = std::move(settings);
}

int PrinterQuery::cookie() const {
  return cookie_;
}

void PrinterQuery::GetDefaultSettings(base::OnceClosure callback,
                                      bool is_modifiable,
                                      bool want_pdf_settings) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Real work is done in PrinterQuery::UseDefaultSettings().
  is_print_dialog_box_shown_ = false;
  if (want_pdf_settings) {
    // `GetPdfSettings()` is always guaranteed to succeed.
    std::unique_ptr<PrintSettings> pdf_settings = GetPdfSettings();
    DCHECK(pdf_settings);
    PostSettingsDone(std::move(callback), is_modifiable,
                     std::move(pdf_settings), mojom::ResultCode::kSuccess);
    return;
  }

  printing_context_->set_margin_type(
      printing::mojom::MarginType::kDefaultMargins);
  // `this` is owned by `callback`, so `base::Unretained()` is safe.
  UseDefaultSettings(base::BindOnce(&PrinterQuery::PostSettingsDone,
                                    base::Unretained(this), std::move(callback),
                                    is_modifiable));
}

void PrinterQuery::GetSettingsFromUser(uint32_t document_page_count,
                                       bool has_selection,
                                       mojom::MarginType margin_type,
                                       bool is_scripted,
                                       bool is_modifiable,
                                       base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!is_print_dialog_box_shown_ || !is_scripted);

  // Real work is done in GetSettingsWithUI().
  is_print_dialog_box_shown_ = true;
  printing_context_->set_margin_type(margin_type);
  // `this` is owned by `callback`, so `base::Unretained()` is safe.
  GetSettingsWithUI(
      document_page_count, has_selection, is_scripted,
      base::BindOnce(&PrinterQuery::PostSettingsDone, base::Unretained(this),
                     std::move(callback), is_modifiable));
}

void PrinterQuery::SetSettings(base::Value::Dict new_settings,
                               base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // `this` is owned by `callback`, so `base::Unretained()` is safe.
  UpdatePrintSettings(
      std::move(new_settings),
      base::BindOnce(&PrinterQuery::PostSettingsDone, base::Unretained(this),
                     std::move(callback),
                     /*maybe_is_modifiable=*/std::nullopt));
}

#if BUILDFLAG(IS_CHROMEOS)
void PrinterQuery::SetSettingsFromPOD(
    std::unique_ptr<PrintSettings> new_settings,
    base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // `this` is owned by `callback`, so `base::Unretained()` is safe.
  UpdatePrintSettingsFromPOD(
      std::move(new_settings),
      base::BindOnce(&PrinterQuery::PostSettingsDone, base::Unretained(this),
                     std::move(callback),
                     /*maybe_is_modifiable=*/std::nullopt));
}
#endif

#if BUILDFLAG(IS_WIN)
void PrinterQuery::UpdatePrintableArea(
    PrintSettings* print_settings,
    OnDidUpdatePrintableAreaCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  scoped_refptr<PrintBackend> print_backend =
      PrintBackend::CreateInstance(g_browser_process->GetApplicationLocale());

  // Blocking is needed here because Windows printer drivers are oftentimes
  // not thread-safe and have to be accessed on the UI thread.
  base::ScopedAllowBlocking allow_blocking;
  std::string printer_name = base::UTF16ToUTF8(print_settings->device_name());
  crash_keys::ScopedPrinterInfo crash_key(
      printer_name, print_backend->GetPrinterDriverInfo(printer_name));

  PRINTER_LOG(EVENT) << "Updating paper printable area in-process for "
                     << printer_name;

  const PrintSettings::RequestedMedia& media =
      print_settings->requested_media();
  std::optional<gfx::Rect> printable_area_um =
      print_backend->GetPaperPrintableArea(printer_name, media.vendor_id,
                                           media.size_microns);
  if (!printable_area_um.has_value()) {
    std::move(callback).Run(/*success=*/false);
    return;
  }

  print_settings->UpdatePrinterPrintableArea(printable_area_um.value());
  std::move(callback).Run(/*success=*/true);
}
#endif

// static
void PrinterQuery::ApplyDefaultPrintableAreaToVirtualPrinterPrintSettings(
    PrintSettings& print_settings) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // The purpose of `print_context` is to set the default printable area. To do
  // so, it doesn't need a RFH, so just default initialize the RFH id.
  PrintingContextDelegate delegate((content::GlobalRenderFrameHostId()));
  std::unique_ptr<PrintingContext> print_context = PrintingContext::Create(
      &delegate, PrintingContext::ProcessBehavior::kOopDisabled);
  print_context->SetPrintSettings(print_settings);
  print_context->SetDefaultPrintableAreaForVirtualPrinters();
  print_settings = print_context->settings();
}

#if BUILDFLAG(ENABLE_OOP_PRINTING)
void PrinterQuery::SetClientId(PrintBackendServiceManager::ClientId client_id) {
  // Only supposed to be called for `PrinterQueryOop` objects.
  NOTREACHED();
}
#endif

std::unique_ptr<PrintSettings> PrinterQuery::GetPdfSettings() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  printing_context_->UsePdfSettings();
  return printing_context_->TakeAndResetSettings();
}

void PrinterQuery::InvokeSettingsCallback(SettingsCallback callback,
                                          mojom::ResultCode result) {
  std::move(callback).Run(printing_context_->TakeAndResetSettings(), result);
}

void PrinterQuery::UpdatePrintSettings(base::Value::Dict new_settings,
                                       SettingsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::unique_ptr<crash_keys::ScopedPrinterInfo> crash_key;
  mojom::PrinterType type = static_cast<mojom::PrinterType>(
      new_settings.FindInt(kSettingPrinterType).value());
  if (type == mojom::PrinterType::kLocal) {
#if BUILDFLAG(IS_WIN)
    // Blocking is needed here because Windows printer drivers are oftentimes
    // not thread-safe and have to be accessed on the UI thread.
    base::ScopedAllowBlocking allow_blocking;
#endif
    scoped_refptr<PrintBackend> print_backend =
        PrintBackend::CreateInstance(g_browser_process->GetApplicationLocale());
    std::string printer_name = *new_settings.FindString(kSettingDeviceName);
    crash_key = std::make_unique<crash_keys::ScopedPrinterInfo>(
        printer_name, print_backend->GetPrinterDriverInfo(printer_name));

#if BUILDFLAG(IS_LINUX) && BUILDFLAG(USE_CUPS)
    PrinterBasicInfo basic_info;
    if (print_backend->GetPrinterBasicInfo(printer_name, &basic_info) ==
        mojom::ResultCode::kSuccess) {
      base::Value::Dict advanced_settings;
      for (const auto& pair : basic_info.options) {
        advanced_settings.Set(pair.first, pair.second);
      }

      new_settings.Set(kSettingAdvancedSettings, std::move(advanced_settings));
    }
#endif  // BUILDFLAG(IS_LINUX) && BUILDFLAG(USE_CUPS)
  }

  mojom::ResultCode result;
  {
#if BUILDFLAG(IS_WIN)
    // Blocking is needed here because Windows printer drivers are oftentimes
    // not thread-safe and have to be accessed on the UI thread.
    base::ScopedAllowBlocking allow_blocking;
#endif
    result = printing_context_->UpdatePrintSettings(std::move(new_settings));
  }

  InvokeSettingsCallback(std::move(callback), result);
}

#if BUILDFLAG(IS_CHROMEOS)
void PrinterQuery::UpdatePrintSettingsFromPOD(
    std::unique_ptr<PrintSettings> new_settings,
    SettingsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  mojom::ResultCode result =
      printing_context_->UpdatePrintSettingsFromPOD(std::move(new_settings));
  InvokeSettingsCallback(std::move(callback), result);
}
#endif

std::unique_ptr<PrintJobWorker> PrinterQuery::CreatePrintJobWorker(
    PrintJob* print_job) {
  return std::make_unique<PrintJobWorker>(std::move(printing_context_delegate_),
                                          std::move(printing_context_),
                                          print_job);
}

void PrinterQuery::GetSettingsWithUI(uint32_t document_page_count,
                                     bool has_selection,
                                     bool is_scripted,
                                     SettingsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (document_page_count > kMaxPageCount) {
    InvokeSettingsCallback(std::move(callback), mojom::ResultCode::kFailed);
    return;
  }

  content::WebContents* web_contents = GetWebContents();

#if BUILDFLAG(IS_ANDROID)
  if (is_scripted) {
    TabAndroid* tab =
        web_contents ? TabAndroid::FromWebContents(web_contents) : nullptr;

    // Regardless of whether the following call fails or not, the javascript
    // call will return since startPendingPrint will make it return immediately
    // in case of error.
    if (tab) {
      auto* printing_context_delegate = static_cast<PrintingContextDelegate*>(
          printing_context_delegate_.get());
      PrintingContextAndroid::SetPendingPrint(
          web_contents->GetTopLevelNativeWindow(),
          GetPrintableForTab(tab->GetJavaObject()),
          printing_context_delegate->rfh_id().child_id,
          printing_context_delegate->rfh_id().frame_routing_id);
    }
  }
#endif

  // Running a dialog causes an exit to webpage-initiated fullscreen.
  // http://crbug.com/728276
  if (web_contents && web_contents->IsFullscreen()) {
    web_contents->ExitFullscreen(true);
  }

  PRINTER_LOG(EVENT) << "Getting printer settings from user in-process";
  printing_context_->AskUserForSettings(
      base::checked_cast<int>(document_page_count), has_selection, is_scripted,
      base::BindOnce(&PrinterQuery::InvokeSettingsCallback,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void PrinterQuery::UseDefaultSettings(SettingsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  PRINTER_LOG(EVENT) << "Using printer default settings in-process";
  mojom::ResultCode result;
  {
#if BUILDFLAG(IS_WIN)
    // Blocking is needed here because Windows printer drivers are oftentimes
    // not thread-safe and have to be accessed on the UI thread.
    base::ScopedAllowBlocking allow_blocking;
#endif
    result = printing_context_->UseDefaultSettings();
  }
  InvokeSettingsCallback(std::move(callback), result);
}

// static
void PrinterQuery::SetCreatePrinterQueryCallbackForTest(
    CreatePrinterQueryCallback* callback) {
  g_create_printer_query_for_testing = callback;
}

bool PrinterQuery::is_valid() const {
  return !!printing_context_;
}

content::WebContents* PrinterQuery::GetWebContents() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  PrintingContextDelegate* printing_context_delegate =
      static_cast<PrintingContextDelegate*>(printing_context_delegate_.get());
  return printing_context_delegate->GetWebContents();
}

}  // namespace printing
