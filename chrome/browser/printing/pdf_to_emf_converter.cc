// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/pdf_to_emf_converter.h"

#include <windows.h>

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/queue.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/printing/printing_service.h"
#include "chrome/services/printing/public/mojom/pdf_to_emf_converter.mojom.h"
#include "chrome/services/printing/public/mojom/printing_service.mojom.h"
#include "components/device_event_log/device_event_log.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "printing/emf_win.h"
#include "printing/pdf_render_settings.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace printing {

namespace {

// Class for converting PDF to another format for printing (Emf, Postscript).
// Class lives on the UI thread.
// Internal workflow is following:
// 1. Create instance on the UI thread.
// 2. Initialize() binds to printing service and start conversion on the UI
//    thread (Mojo actually makes that happen transparently on the IO thread).
// 3. Printing service returns page count.
// 4. For each page:
//   1. Clients requests page.
//   2. Utility converts the page, and sends back the data in a memory region.
class PdfConverterImpl : public PdfConverter {
 public:
  PdfConverterImpl(scoped_refptr<base::RefCountedMemory> data,
                   const PdfRenderSettings& conversion_settings,
                   const std::optional<bool>& use_skia,
                   const GURL& url,
                   StartCallback start_callback);

  PdfConverterImpl(const PdfConverterImpl&) = delete;
  PdfConverterImpl& operator=(const PdfConverterImpl&) = delete;

  ~PdfConverterImpl() override;

  static void set_fail_when_initializing_conversion_for_tests(bool fail) {
    simulate_failure_initializing_conversion_ = fail;
  }

  static bool fail_when_initializing_conversion_for_tests() {
    return simulate_failure_initializing_conversion_;
  }

 private:
  class GetPageCallbackData {
   public:
    GetPageCallbackData(uint32_t page_index,
                        PdfConverter::GetPageCallback callback)
        : page_index_(page_index), callback_(callback) {}

    GetPageCallbackData(const GetPageCallbackData&) = delete;
    GetPageCallbackData& operator=(const GetPageCallbackData&) = delete;

    GetPageCallbackData(GetPageCallbackData&& other) {
      *this = std::move(other);
    }

    GetPageCallbackData& operator=(GetPageCallbackData&& rhs) {
      page_index_ = rhs.page_index_;
      callback_ = rhs.callback_;
      return *this;
    }

    uint32_t page_index() const { return page_index_; }

    PdfConverter::GetPageCallback callback() const { return callback_; }

   private:
    uint32_t page_index_;

    PdfConverter::GetPageCallback callback_;
  };

  void Initialize(scoped_refptr<base::RefCountedMemory> data);

  void GetPage(uint32_t page_index,
               PdfConverter::GetPageCallback get_page_callback) override;

  void Stop();

  std::unique_ptr<MetafilePlayer> GetMetaFileFromMapping(
      base::ReadOnlySharedMemoryMapping mapping);

  void OnPageCount(mojo::PendingRemote<mojom::PdfToEmfConverter> converter,
                   uint32_t page_count);
  void OnPageDone(base::ReadOnlySharedMemoryRegion emf_region,
                  float scale_factor);

  void OnFailed(std::string_view error_message);

  void RecordConversionMetrics();

  const PdfRenderSettings settings_;

  std::optional<bool> use_skia_;

  const GURL url_;

  // Document loaded callback.
  PdfConverter::StartCallback start_callback_;

  // Queue of callbacks for GetPage() requests. Utility process should reply
  // with PageDone in the same order as requests were received.
  // Use containers that keeps element pointers valid after push() and pop().
  using GetPageCallbacks = base::queue<GetPageCallbackData>;
  GetPageCallbacks get_page_callbacks_;

  // Keep track of document size and page counts for metrics.
  size_t bytes_generated_ = 0;
  uint32_t pages_generated_ = 0;
  uint32_t page_count_ = 0;

  mojo::Remote<mojom::PdfToEmfConverter> pdf_to_emf_converter_;

  mojo::Remote<mojom::PdfToEmfConverterFactory> pdf_to_emf_converter_factory_;

  base::WeakPtrFactory<PdfConverterImpl> weak_ptr_factory_{this};

  static bool simulate_failure_initializing_conversion_;
};

// static
bool PdfConverterImpl::simulate_failure_initializing_conversion_ = false;

std::unique_ptr<MetafilePlayer> PdfConverterImpl::GetMetaFileFromMapping(
    base::ReadOnlySharedMemoryMapping mapping) {
  std::unique_ptr<Emf> metafile;
  if (settings_.mode == PdfRenderSettings::Mode::POSTSCRIPT_LEVEL2 ||
      settings_.mode == PdfRenderSettings::Mode::POSTSCRIPT_LEVEL3 ||
      settings_.mode ==
          PdfRenderSettings::Mode::POSTSCRIPT_LEVEL3_WITH_TYPE42_FONTS ||
      settings_.mode == PdfRenderSettings::Mode::TEXTONLY) {
    metafile = std::make_unique<PostScriptMetaFile>();
  } else {
    metafile = std::make_unique<Emf>();
  }
  if (!metafile->InitFromData(mapping.GetMemoryAsSpan<const uint8_t>()))
    metafile.reset();
  return metafile;
}

PdfConverterImpl::PdfConverterImpl(scoped_refptr<base::RefCountedMemory> data,
                                   const PdfRenderSettings& settings,
                                   const std::optional<bool>& use_skia,
                                   const GURL& url,
                                   StartCallback start_callback)
    : settings_(settings),
      use_skia_(use_skia),
      url_(url),
      start_callback_(std::move(start_callback)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(start_callback_);

  Initialize(data);
}

PdfConverterImpl::~PdfConverterImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  RecordConversionMetrics();
}

void PdfConverterImpl::Initialize(scoped_refptr<base::RefCountedMemory> data) {
  if (simulate_failure_initializing_conversion_) {
    OnFailed("Failed to create PDF data mapping.");
    return;
  }

  base::MappedReadOnlyRegion memory =
      base::ReadOnlySharedMemoryRegion::Create(data->size());
  if (!memory.IsValid()) {
    OnFailed("Failed to create PDF data mapping.");
    return;
  }

  PRINTER_LOG(EVENT) << "PdfConverter created. Mode: " << settings_.mode;
  memcpy(memory.mapping.memory(), data->front(), data->size());

  GetPrintingService()->BindPdfToEmfConverterFactory(
      pdf_to_emf_converter_factory_.BindNewPipeAndPassReceiver());
  pdf_to_emf_converter_factory_.set_disconnect_handler(base::BindOnce(
      &PdfConverterImpl::OnFailed, weak_ptr_factory_.GetWeakPtr(),
      "Connection to PdfToEmfConverterFactory error."));

  pdf_to_emf_converter_factory_->CreateConverter(
      std::move(memory.region), settings_,
      base::BindOnce(&PdfConverterImpl::OnPageCount,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PdfConverterImpl::OnPageCount(
    mojo::PendingRemote<mojom::PdfToEmfConverter> converter,
    uint32_t page_count) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!pdf_to_emf_converter_.is_bound());
  pdf_to_emf_converter_.Bind(std::move(converter));
  pdf_to_emf_converter_.set_disconnect_handler(base::BindOnce(
      &PdfConverterImpl::OnFailed, weak_ptr_factory_.GetWeakPtr(),
      "Connection to PdfToEmfConverter error."));
  pdf_to_emf_converter_->SetWebContentsURL(url_);
  if (use_skia_) {
    pdf_to_emf_converter_->SetUseSkiaRendererPolicy(*use_skia_);
  }
  std::move(start_callback_).Run(page_count);
  page_count_ = page_count;
}

void PdfConverterImpl::GetPage(
    uint32_t page_index,
    PdfConverter::GetPageCallback get_page_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(pdf_to_emf_converter_.is_bound());

  // Store callback before any OnFailed() call to make it called on failure.
  get_page_callbacks_.push(GetPageCallbackData(page_index, get_page_callback));

  if (!pdf_to_emf_converter_)
    return OnFailed("No PdfToEmfConverter.");

  pdf_to_emf_converter_->ConvertPage(
      page_index, base::BindOnce(&PdfConverterImpl::OnPageDone,
                                 weak_ptr_factory_.GetWeakPtr()));
}

void PdfConverterImpl::OnPageDone(base::ReadOnlySharedMemoryRegion emf_region,
                                  float scale_factor) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (get_page_callbacks_.empty())
    return OnFailed("No get_page callbacks.");

  GetPageCallbackData& data = get_page_callbacks_.front();
  std::unique_ptr<MetafilePlayer> metafile;
  if (emf_region.IsValid()) {
    base::ReadOnlySharedMemoryMapping mapping = emf_region.Map();
    if (mapping.IsValid()) {
      size_t mapping_size = mapping.size();
      metafile = GetMetaFileFromMapping(std::move(mapping));
      if (metafile) {
        ++pages_generated_;
        bytes_generated_ += mapping_size;
      }
    }
  }

  base::WeakPtr<PdfConverterImpl> weak_this = weak_ptr_factory_.GetWeakPtr();
  data.callback().Run(data.page_index(), scale_factor, std::move(metafile));
  // WARNING: the callback might have deleted `this`!
  if (!weak_this)
    return;
  get_page_callbacks_.pop();
}

void PdfConverterImpl::Stop() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Disconnect interface ptrs so that the printing service process stop.
  pdf_to_emf_converter_factory_.reset();
  pdf_to_emf_converter_.reset();
}

void PdfConverterImpl::OnFailed(std::string_view error_message) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  LOG(ERROR) << "Failed to convert PDF: " << error_message;
  base::WeakPtr<PdfConverterImpl> weak_this = weak_ptr_factory_.GetWeakPtr();
  if (start_callback_) {
    std::move(start_callback_).Run(/*page_count=*/0);
    if (!weak_this)
      return;  // Protect against the `start_callback_` deleting `this`.
  }

  while (!get_page_callbacks_.empty()) {
    OnPageDone(base::ReadOnlySharedMemoryRegion(), 0.0f);
    if (!weak_this) {
      // OnPageDone invokes the GetPageCallback which might end up deleting
      // this.
      return;
    }
  }

  Stop();
}

void PdfConverterImpl::RecordConversionMetrics() {
  if (!page_count_ || page_count_ != pages_generated_) {
    // TODO(thestig): Consider adding UMA to track failure rates.
    return;
  }

  DCHECK(bytes_generated_);
  size_t average_page_size_in_kb = bytes_generated_ / 1024;
  average_page_size_in_kb /= page_count_;
  switch (settings_.mode) {
    case PdfRenderSettings::Mode::NORMAL:
      UMA_HISTOGRAM_MEMORY_KB("Printing.ConversionSize.Emf",
                              average_page_size_in_kb);
      return;
    case PdfRenderSettings::Mode::TEXTONLY:
      // Intentionally not logged.
      return;
    case PdfRenderSettings::Mode::POSTSCRIPT_LEVEL2:
      UMA_HISTOGRAM_MEMORY_KB("Printing.ConversionSize.PostScript2",
                              average_page_size_in_kb);
      return;
    case PdfRenderSettings::Mode::POSTSCRIPT_LEVEL3:
      UMA_HISTOGRAM_MEMORY_KB("Printing.ConversionSize.PostScript3",
                              average_page_size_in_kb);
      return;
    case PdfRenderSettings::Mode::EMF_WITH_REDUCED_RASTERIZATION:
      UMA_HISTOGRAM_MEMORY_KB(
          "Printing.ConversionSize.EmfWithReducedRasterization",
          average_page_size_in_kb);
      return;
    case PdfRenderSettings::Mode::POSTSCRIPT_LEVEL3_WITH_TYPE42_FONTS:
      UMA_HISTOGRAM_MEMORY_KB(
          "Printing.ConversionSize.PostScript3WithType42Fonts",
          average_page_size_in_kb);
      return;
  }
}

}  // namespace

PdfConverter::~PdfConverter() = default;

// static
std::unique_ptr<PdfConverter> PdfConverter::StartPdfConverter(
    scoped_refptr<base::RefCountedMemory> data,
    const PdfRenderSettings& conversion_settings,
    const std::optional<bool>& use_skia,
    const GURL& url,
    StartCallback start_callback) {
  return std::make_unique<PdfConverterImpl>(data, conversion_settings, use_skia,
                                            url, std::move(start_callback));
}

ScopedSimulateFailureCreatingTempFileForTests::
    ScopedSimulateFailureCreatingTempFileForTests() {
  PdfConverterImpl::set_fail_when_initializing_conversion_for_tests(true);
}

ScopedSimulateFailureCreatingTempFileForTests::
    ~ScopedSimulateFailureCreatingTempFileForTests() {
  PdfConverterImpl::set_fail_when_initializing_conversion_for_tests(false);
}

}  // namespace printing
