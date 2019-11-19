// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/pdf_to_emf_converter.h"

#include <stdint.h>
#include <windows.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/queue.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/printing/printing_service.h"
#include "chrome/services/printing/public/mojom/pdf_to_emf_converter.mojom.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "printing/emf_win.h"
#include "printing/pdf_render_settings.h"

using content::BrowserThread;

namespace printing {

namespace {

class PdfToEmfConverterClientImpl : public mojom::PdfToEmfConverterClient {
 public:
  explicit PdfToEmfConverterClientImpl(
      mojo::PendingReceiver<mojom::PdfToEmfConverterClient> receiver)
      : receiver_(this, std::move(receiver)) {}

 private:
  // mojom::PdfToEmfConverterClient implementation.
  void PreCacheFontCharacters(
      const std::vector<uint8_t>& logfont_data,
      const base::string16& characters,
      PreCacheFontCharactersCallback callback) override {
    // TODO(scottmg): pdf/ppapi still require the renderer to be able to
    // precache GDI fonts (http://crbug.com/383227), even when using
    // DirectWrite. Eventually this shouldn't be added and should be moved to
    // FontCacheDispatcher too. http://crbug.com/356346.

    // First, comments from FontCacheDispatcher::OnPreCacheFont do apply here
    // too. Except that for True Type fonts, GetTextMetrics will not load the
    // font in memory. The only way windows seem to load properly, it is to
    // create a similar device (like the one in which we print), then do an
    // ExtTextOut, as we do in the printing thread, which is sandboxed.
    const LOGFONT* logfont =
        reinterpret_cast<const LOGFONT*>(&logfont_data.at(0));

    HDC hdc = CreateEnhMetaFile(nullptr, nullptr, nullptr, nullptr);
    HFONT font_handle = CreateFontIndirect(logfont);
    DCHECK(font_handle != nullptr);

    HGDIOBJ old_font = SelectObject(hdc, font_handle);
    DCHECK(old_font != nullptr);

    ExtTextOut(hdc, 0, 0, ETO_GLYPH_INDEX, 0, characters.c_str(),
               characters.length(), nullptr);

    SelectObject(hdc, old_font);
    DeleteObject(font_handle);

    HENHMETAFILE metafile = CloseEnhMetaFile(hdc);

    if (metafile)
      DeleteEnhMetaFile(metafile);

    std::move(callback).Run();
  }

  mojo::Receiver<mojom::PdfToEmfConverterClient> receiver_;
};

// Emf subclass that knows how to play back PostScript data embedded as EMF
// comment records.
class PostScriptMetaFile : public Emf {
 public:
  PostScriptMetaFile() {}
  ~PostScriptMetaFile() override {}

 private:
  // Emf:
  bool SafePlayback(HDC hdc) const override;

  DISALLOW_COPY_AND_ASSIGN(PostScriptMetaFile);
};

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
                   StartCallback start_callback);
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
    GetPageCallbackData(int page_number, PdfConverter::GetPageCallback callback)
        : page_number_(page_number), callback_(callback) {}

    GetPageCallbackData(GetPageCallbackData&& other) {
      *this = std::move(other);
    }

    GetPageCallbackData& operator=(GetPageCallbackData&& rhs) {
      page_number_ = rhs.page_number_;
      callback_ = rhs.callback_;
      return *this;
    }

    int page_number() const { return page_number_; }

    PdfConverter::GetPageCallback callback() const { return callback_; }

   private:
    int page_number_;

    PdfConverter::GetPageCallback callback_;

    DISALLOW_COPY_AND_ASSIGN(GetPageCallbackData);
  };

  void Initialize(scoped_refptr<base::RefCountedMemory> data);

  void GetPage(int page_number,
               PdfConverter::GetPageCallback get_page_callback) override;

  void Stop();

  std::unique_ptr<MetafilePlayer> GetMetaFileFromMapping(
      base::ReadOnlySharedMemoryMapping mapping);

  void OnPageCount(mojo::PendingRemote<mojom::PdfToEmfConverter> converter,
                   uint32_t page_count);
  void OnPageDone(base::ReadOnlySharedMemoryRegion emf_region,
                  float scale_factor);

  void OnFailed(const std::string& error_message);

  void RecordConversionMetrics();

  PdfRenderSettings settings_;

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

  std::unique_ptr<PdfToEmfConverterClientImpl>
      pdf_to_emf_converter_client_impl_;

  mojo::Remote<mojom::PdfToEmfConverter> pdf_to_emf_converter_;

  mojo::Remote<mojom::PdfToEmfConverterFactory> pdf_to_emf_converter_factory_;

  base::WeakPtrFactory<PdfConverterImpl> weak_ptr_factory_{this};

  static bool simulate_failure_initializing_conversion_;

  DISALLOW_COPY_AND_ASSIGN(PdfConverterImpl);
};

// static
bool PdfConverterImpl::simulate_failure_initializing_conversion_ = false;

std::unique_ptr<MetafilePlayer> PdfConverterImpl::GetMetaFileFromMapping(
    base::ReadOnlySharedMemoryMapping mapping) {
  std::unique_ptr<Emf> metafile;
  if (settings_.mode == PdfRenderSettings::Mode::POSTSCRIPT_LEVEL2 ||
      settings_.mode == PdfRenderSettings::Mode::POSTSCRIPT_LEVEL3 ||
      settings_.mode == PdfRenderSettings::Mode::TEXTONLY) {
    metafile = std::make_unique<PostScriptMetaFile>();
  } else {
    metafile = std::make_unique<Emf>();
  }
  if (!metafile->InitFromData(mapping.memory(), mapping.size()))
    metafile.reset();
  return metafile;
}

bool PostScriptMetaFile::SafePlayback(HDC hdc) const {
  Emf::Enumerator emf_enum(*this, nullptr, nullptr);
  for (const Emf::Record& record : emf_enum) {
    auto* emf_record = record.record();
    if (emf_record->iType != EMR_GDICOMMENT)
      continue;

    const EMRGDICOMMENT* comment =
        reinterpret_cast<const EMRGDICOMMENT*>(emf_record);
    const char* data = reinterpret_cast<const char*>(comment->Data);
    const uint16_t* ptr = reinterpret_cast<const uint16_t*>(data);
    int ret = ExtEscape(hdc, PASSTHROUGH, 2 + *ptr, data, 0, nullptr);
    DCHECK_EQ(*ptr, ret);
  }
  return true;
}

PdfConverterImpl::PdfConverterImpl(scoped_refptr<base::RefCountedMemory> data,
                                   const PdfRenderSettings& settings,
                                   StartCallback start_callback)
    : settings_(settings), start_callback_(std::move(start_callback)) {
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
    OnFailed(std::string("Failed to create PDF data mapping."));
    return;
  }

  base::MappedReadOnlyRegion memory =
      base::ReadOnlySharedMemoryRegion::Create(data->size());
  if (!memory.region.IsValid() || !memory.mapping.IsValid()) {
    OnFailed(std::string("Failed to create PDF data mapping."));
    return;
  }

  memcpy(memory.mapping.memory(), data->front(), data->size());

  GetPrintingService()->BindPdfToEmfConverterFactory(
      pdf_to_emf_converter_factory_.BindNewPipeAndPassReceiver());
  pdf_to_emf_converter_factory_.set_disconnect_handler(base::BindOnce(
      &PdfConverterImpl::OnFailed, weak_ptr_factory_.GetWeakPtr(),
      std::string("Connection to PdfToEmfConverterFactory error.")));

  mojo::PendingRemote<mojom::PdfToEmfConverterClient>
      pdf_to_emf_converter_client_remote;
  pdf_to_emf_converter_client_impl_ =
      std::make_unique<PdfToEmfConverterClientImpl>(
          pdf_to_emf_converter_client_remote.InitWithNewPipeAndPassReceiver());

  pdf_to_emf_converter_factory_->CreateConverter(
      std::move(memory.region), settings_,
      std::move(pdf_to_emf_converter_client_remote),
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
      std::string("Connection to PdfToEmfConverter error.")));
  std::move(start_callback_).Run(page_count);
  page_count_ = page_count;
}

void PdfConverterImpl::GetPage(
    int page_number,
    PdfConverter::GetPageCallback get_page_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(pdf_to_emf_converter_.is_bound());

  // Store callback before any OnFailed() call to make it called on failure.
  get_page_callbacks_.push(GetPageCallbackData(page_number, get_page_callback));

  if (!pdf_to_emf_converter_)
    return OnFailed(std::string("No PdfToEmfConverter."));

  pdf_to_emf_converter_->ConvertPage(
      page_number, base::BindOnce(&PdfConverterImpl::OnPageDone,
                                  weak_ptr_factory_.GetWeakPtr()));
}

void PdfConverterImpl::OnPageDone(base::ReadOnlySharedMemoryRegion emf_region,
                                  float scale_factor) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (get_page_callbacks_.empty())
    return OnFailed(std::string("No get_page callbacks."));

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
  data.callback().Run(data.page_number(), scale_factor, std::move(metafile));
  // WARNING: the callback might have deleted |this|!
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

void PdfConverterImpl::OnFailed(const std::string& error_message) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  LOG(ERROR) << "Failed to convert PDF: " << error_message;
  base::WeakPtr<PdfConverterImpl> weak_this = weak_ptr_factory_.GetWeakPtr();
  if (!start_callback_.is_null()) {
    std::move(start_callback_).Run(/*page_count=*/0);
    if (!weak_this)
      return;  // Protect against the |start_callback_| deleting |this|.
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
    case PdfRenderSettings::Mode::GDI_TEXT:
      UMA_HISTOGRAM_MEMORY_KB("Printing.ConversionSize.EmfWithGdiText",
                              average_page_size_in_kb);
      return;
    case PdfRenderSettings::Mode::POSTSCRIPT_LEVEL2:
      UMA_HISTOGRAM_MEMORY_KB("Printing.ConversionSize.PostScript2",
                              average_page_size_in_kb);
      return;
    case PdfRenderSettings::Mode::POSTSCRIPT_LEVEL3:
      UMA_HISTOGRAM_MEMORY_KB("Printing.ConversionSize.PostScript3",
                              average_page_size_in_kb);
      return;
    default:
      NOTREACHED();
      return;
  }
}

}  // namespace

PdfConverter::~PdfConverter() = default;

// static
std::unique_ptr<PdfConverter> PdfConverter::StartPdfConverter(
    scoped_refptr<base::RefCountedMemory> data,
    const PdfRenderSettings& conversion_settings,
    StartCallback start_callback) {
  return std::make_unique<PdfConverterImpl>(data, conversion_settings,
                                            std::move(start_callback));
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
