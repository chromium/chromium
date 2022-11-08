// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/current_process.h"

namespace base {

namespace {

struct ProcessName {
  CurrentProcessType type;
  const char* name;
};

constexpr ProcessName kProcessNames[] = {
    {CurrentProcessType::PROCESS_UNSPECIFIED, "Null"},
    {CurrentProcessType::PROCESS_BROWSER, "Browser"},
    {CurrentProcessType::PROCESS_RENDERER, "Renderer"},
    {CurrentProcessType::PROCESS_UTILITY, "Utility"},
    {CurrentProcessType::PROCESS_ZYGOTE, "SandboxHelper"},
    {CurrentProcessType::PROCESS_GPU, "Gpu"},
    {CurrentProcessType::PROCESS_PPAPI_PLUGIN, "PpapiPlugin"},
    {CurrentProcessType::PROCESS_PPAPI_BROKER, "PpapiBroker"},
    {CurrentProcessType::PROCESS_SERVICE_NETWORK,
     "Service: network.mojom.NetworkService"},
    {CurrentProcessType::PROCESS_SERVICE_TRACING,
     "Service: tracing.mojom.TracingService"},
    {CurrentProcessType::PROCESS_SERVICE_STORAGE,
     "Service: storage.mojom.StorageService"},
    {CurrentProcessType::PROCESS_SERVICE_AUDIO,
     "Service: audio.mojom.AudioService"},
    {CurrentProcessType::PROCESS_SERVICE_DATA_DECODER,
     "Service: data_decoder.mojom.DataDecoderService"},
    {CurrentProcessType::PROCESS_SERVICE_UTIL_WIN,
     "Service: chrome.mojom.UtilWin"},
    {CurrentProcessType::PROCESS_SERVICE_PROXY_RESOLVER,
     "Service: proxy_resolver.mojom.ProxyResolverFactory"},
    {CurrentProcessType::PROCESS_SERVICE_CDM,
     "Service: media.mojom.CdmService"},
    {CurrentProcessType::PROCESS_SERVICE_VIDEO_CAPTURE,
     "Service: video_capture.mojom.VideoCaptureService"},
    {CurrentProcessType::PROCESS_SERVICE_UNZIPPER,
     "Service: unzip.mojom.Unzipper"},
    {CurrentProcessType::PROCESS_SERVICE_MIRRORING,
     "Service: mirroring.mojom.MirroringService"},
    {CurrentProcessType::PROCESS_SERVICE_FILEPATCHER,
     "Service: patch.mojom.FilePatcher"},
    {CurrentProcessType::PROCESS_SERVICE_TTS,
     "Service: chromeos.tts.mojom.TtsService"},
    {CurrentProcessType::PROCESS_SERVICE_PRINTING,
     "Service: printing.mojom.PrintingService"},
    {CurrentProcessType::PROCESS_SERVICE_QUARANTINE,
     "Service: quarantine.mojom.Quarantine"},
    {CurrentProcessType::PROCESS_SERVICE_CROS_LOCALSEARCH,
     "Service: chromeos.local_search_service.mojom.LocalSearchService"},
    {CurrentProcessType::PROCESS_SERVICE_CROS_ASSISTANT_AUDIO_DECODER,
     "Service: chromeos.assistant.mojom.AssistantAudioDecoderFactory"},
    {CurrentProcessType::PROCESS_SERVICE_FILEUTIL,
     "Service: chrome.mojom.FileUtilService"},
    {CurrentProcessType::PROCESS_SERVICE_PRINTCOMPOSITOR,
     "Service: printing.mojom.PrintCompositor"},
    {CurrentProcessType::PROCESS_SERVICE_PAINTPREVIEW,
     "Service: paint_preview.mojom.PaintPreviewCompositorCollection"},
    {CurrentProcessType::PROCESS_SERVICE_SPEECHRECOGNITION,
     "Service: media.mojom.SpeechRecognitionService"},
    {CurrentProcessType::PROCESS_SERVICE_XRDEVICE,
     "Service: device.mojom.XRDeviceService"},
    {CurrentProcessType::PROCESS_SERVICE_READICON,
     "Service: chrome.mojom.UtilReadIcon"},
    {CurrentProcessType::PROCESS_SERVICE_LANGUAGEDETECTION,
     "Service: language_detection.mojom.LanguageDetectionService"},
    {CurrentProcessType::PROCESS_SERVICE_SHARING,
     "Service: sharing.mojom.Sharing"},
    {CurrentProcessType::PROCESS_SERVICE_MEDIAPARSER,
     "Service: chrome.mojom.MediaParserFactory"},
    {CurrentProcessType::PROCESS_SERVICE_QRCODEGENERATOR,
     "Service: qrcode_generator.mojom.QRCodeGeneratorService"},
    {CurrentProcessType::PROCESS_SERVICE_PROFILEIMPORT,
     "Service: chrome.mojom.ProfileImport"},
    {CurrentProcessType::PROCESS_SERVICE_IME,
     "Service: chromeos.ime.mojom.ImeService"},
    {CurrentProcessType::PROCESS_SERVICE_RECORDING,
     "Service: recording.mojom.RecordingService"},
    {CurrentProcessType::PROCESS_SERVICE_SHAPEDETECTION,
     "Service: shape_detection.mojom.ShapeDetectionService"},
    {CurrentProcessType::PROCESS_RENDERER_EXTENSION, "Extension Renderer"},
};

}  // namespace

// static
CurrentProcess& CurrentProcess::GetInstance() {
  static base::NoDestructor<CurrentProcess> instance;
  return *instance;
}

void CurrentProcess::SetProcessType(CurrentProcessType process_type) {
  std::string process_name;
  for (size_t i = 0; i < std::size(kProcessNames); ++i) {
    if (process_type == kProcessNames[i].type) {
      process_name = kProcessNames[i].name;
    }
  }
  CurrentProcess::GetInstance().SetProcessNameAndType(process_name,
                                                      process_type);
}

void CurrentProcess::SetProcessNameAndType(const std::string& process_name,
                                           CurrentProcessType process_type) {
  {
    AutoLock lock(lock_);
    process_name_ = process_name;
    process_type_.store(static_cast<CurrentProcessType>(process_type),
                        std::memory_order_relaxed);
  }
#if BUILDFLAG(ENABLE_BASE_TRACING)
  trace_event::TraceLog::GetInstance()->set_process_name(process_name);
#endif
}

}  // namespace base
