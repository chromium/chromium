// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/current_process.h"

namespace base {

namespace {

const char* GetNameForProcessType(CurrentProcessType process_type) {
#if BUILDFLAG(ENABLE_BASE_TRACING)
  switch (process_type) {
    case CurrentProcessType::PROCESS_UNSPECIFIED:
      return "Null";
    case CurrentProcessType::PROCESS_BROWSER:
      return "Browser";
    case CurrentProcessType::PROCESS_RENDERER:
      return "Renderer";
    case CurrentProcessType::PROCESS_UTILITY:
      return "Utility";
    case CurrentProcessType::PROCESS_ZYGOTE:
      return "Zygote";
    case CurrentProcessType::PROCESS_SANDBOX_HELPER:
      return "SandboxHelper";
    case CurrentProcessType::PROCESS_GPU:
      return "GPU Process";
    case CurrentProcessType::PROCESS_PPAPI_PLUGIN:
      return "PPAPI Process";
    case CurrentProcessType::PROCESS_PPAPI_BROKER:
      return "PPAPI Broker Process";
    case CurrentProcessType::PROCESS_SERVICE_NETWORK:
      return "Service: network.mojom.NetworkService";
    case CurrentProcessType::PROCESS_SERVICE_TRACING:
      return "Service: tracing.mojom.TracingService";
    case CurrentProcessType::PROCESS_SERVICE_STORAGE:
      return "Service: storage.mojom.StorageService";
    case CurrentProcessType::PROCESS_SERVICE_AUDIO:
      return "Service: audio.mojom.AudioService";
    case CurrentProcessType::PROCESS_SERVICE_DATA_DECODER:
      return "Service: data_decoder.mojom.DataDecoderService";
    case CurrentProcessType::PROCESS_SERVICE_UTIL_WIN:
      return "Service: chrome.mojom.UtilWin";
    case CurrentProcessType::PROCESS_SERVICE_PROXY_RESOLVER:
      return "Service: proxy_resolver.mojom.ProxyResolverFactory";
    case CurrentProcessType::PROCESS_SERVICE_CDM:
      return "Service: media.mojom.CdmServiceBroker";
    case CurrentProcessType::PROCESS_SERVICE_MEDIA_FOUNDATION:
      return "Service: media.mojom.MediaFoundationServiceBroker";
    case CurrentProcessType::PROCESS_SERVICE_VIDEO_CAPTURE:
      return "Service: video_capture.mojom.VideoCaptureService";
    case CurrentProcessType::PROCESS_SERVICE_UNZIPPER:
      return "Service: unzip.mojom.Unzipper";
    case CurrentProcessType::PROCESS_SERVICE_MIRRORING:
      return "Service: mirroring.mojom.MirroringService";
    case CurrentProcessType::PROCESS_SERVICE_FILEPATCHER:
      return "Service: patch.mojom.FilePatcher";
    case CurrentProcessType::PROCESS_SERVICE_TTS:
      return "Service: chromeos.tts.mojom.TtsService";
    case CurrentProcessType::PROCESS_SERVICE_PRINTING:
      return "Service: printing.mojom.PrintingService";
    case CurrentProcessType::PROCESS_SERVICE_QUARANTINE:
      return "Service: quarantine.mojom.Quarantine";
    case CurrentProcessType::PROCESS_SERVICE_CROS_LOCALSEARCH:
      return "Service: chromeos.local_search_service.mojom.LocalSearchService";
    case CurrentProcessType::PROCESS_SERVICE_CROS_ASSISTANT_AUDIO_DECODER:
      return "Service: chromeos.assistant.mojom.AssistantAudioDecoderFactory";
    case CurrentProcessType::PROCESS_SERVICE_FILEUTIL:
      return "Service: chrome.mojom.FileUtilService";
    case CurrentProcessType::PROCESS_SERVICE_PRINTCOMPOSITOR:
      return "Service: printing.mojom.PrintCompositor";
    case CurrentProcessType::PROCESS_SERVICE_PAINTPREVIEW:
      return "Service: paint_preview.mojom.PaintPreviewCompositorCollection";
    case CurrentProcessType::PROCESS_SERVICE_SPEECHRECOGNITION:
      return "Service: media.mojom.SpeechRecognitionService";
    case CurrentProcessType::PROCESS_SERVICE_XRDEVICE:
      return "Service: device.mojom.XRDeviceService";
    case CurrentProcessType::PROCESS_SERVICE_READICON:
      return "Service: chrome.mojom.UtilReadIcon";
    case CurrentProcessType::PROCESS_SERVICE_LANGUAGEDETECTION:
      return "Service: language_detection.mojom.LanguageDetectionService";
    case CurrentProcessType::PROCESS_SERVICE_SHARING:
      return "Service: sharing.mojom.Sharing";
    case CurrentProcessType::PROCESS_SERVICE_MEDIAPARSER:
      return "Service: chrome.mojom.MediaParserFactory";
    case CurrentProcessType::PROCESS_SERVICE_QRCODEGENERATOR:
      return "Service: qrcode_generator.mojom.QRCodeGeneratorService";
    case CurrentProcessType::PROCESS_SERVICE_PROFILEIMPORT:
      return "Service: chrome.mojom.ProfileImport";
    case CurrentProcessType::PROCESS_SERVICE_IME:
      return "Service: chromeos.ime.mojom.ImeService";
    case CurrentProcessType::PROCESS_SERVICE_RECORDING:
      return "Service: recording.mojom.RecordingService";
    case CurrentProcessType::PROCESS_SERVICE_SHAPEDETECTION:
      return "Service: shape_detection.mojom.ShapeDetectionService";
    case CurrentProcessType::PROCESS_RENDERER_EXTENSION:
      return "Extension Renderer";
  }
#else
  return "Null";
#endif  // BUILDFLAG(ENABLE_BASE_TRACING)
}

}  // namespace

// Used for logging histograms for IPC metrics based on their process type.
ShortProcessType CurrentProcess::GetShortType(TypeKey key) {
#if BUILDFLAG(ENABLE_BASE_TRACING)
  CurrentProcessType process = static_cast<CurrentProcessType>(
      process_type_.load(std::memory_order_relaxed));
  switch (process) {
    case CurrentProcessType::PROCESS_UNSPECIFIED:
      return ShortProcessType::kUnspecified;
    case CurrentProcessType::PROCESS_BROWSER:
      return ShortProcessType::kBrowser;
    case CurrentProcessType::PROCESS_RENDERER:
      return ShortProcessType::kRenderer;
    case CurrentProcessType::PROCESS_UTILITY:
      return ShortProcessType::kUtility;
    case CurrentProcessType::PROCESS_ZYGOTE:
      return ShortProcessType::kZygote;
    case CurrentProcessType::PROCESS_SANDBOX_HELPER:
      return ShortProcessType::kSandboxHelper;
    case CurrentProcessType::PROCESS_GPU:
      return ShortProcessType::kGpu;
    case CurrentProcessType::PROCESS_PPAPI_PLUGIN:
      return ShortProcessType::kPpapiPlugin;
    case CurrentProcessType::PROCESS_PPAPI_BROKER:
      return ShortProcessType::kPpapiBroker;
    case CurrentProcessType::PROCESS_SERVICE_NETWORK:
      return ShortProcessType::kServiceNetwork;
    case CurrentProcessType::PROCESS_SERVICE_STORAGE:
      return ShortProcessType::kServiceStorage;
    case CurrentProcessType::PROCESS_RENDERER_EXTENSION:
      return ShortProcessType::kRendererExtension;
    case CurrentProcessType::PROCESS_SERVICE_TRACING:
    case CurrentProcessType::PROCESS_SERVICE_AUDIO:
    case CurrentProcessType::PROCESS_SERVICE_DATA_DECODER:
    case CurrentProcessType::PROCESS_SERVICE_UTIL_WIN:
    case CurrentProcessType::PROCESS_SERVICE_PROXY_RESOLVER:
    case CurrentProcessType::PROCESS_SERVICE_CDM:
    case CurrentProcessType::PROCESS_SERVICE_MEDIA_FOUNDATION:
    case CurrentProcessType::PROCESS_SERVICE_VIDEO_CAPTURE:
    case CurrentProcessType::PROCESS_SERVICE_UNZIPPER:
    case CurrentProcessType::PROCESS_SERVICE_MIRRORING:
    case CurrentProcessType::PROCESS_SERVICE_FILEPATCHER:
    case CurrentProcessType::PROCESS_SERVICE_TTS:
    case CurrentProcessType::PROCESS_SERVICE_PRINTING:
    case CurrentProcessType::PROCESS_SERVICE_QUARANTINE:
    case CurrentProcessType::PROCESS_SERVICE_CROS_LOCALSEARCH:
    case CurrentProcessType::PROCESS_SERVICE_CROS_ASSISTANT_AUDIO_DECODER:
    case CurrentProcessType::PROCESS_SERVICE_FILEUTIL:
    case CurrentProcessType::PROCESS_SERVICE_PRINTCOMPOSITOR:
    case CurrentProcessType::PROCESS_SERVICE_PAINTPREVIEW:
    case CurrentProcessType::PROCESS_SERVICE_SPEECHRECOGNITION:
    case CurrentProcessType::PROCESS_SERVICE_XRDEVICE:
    case CurrentProcessType::PROCESS_SERVICE_READICON:
    case CurrentProcessType::PROCESS_SERVICE_LANGUAGEDETECTION:
    case CurrentProcessType::PROCESS_SERVICE_SHARING:
    case CurrentProcessType::PROCESS_SERVICE_MEDIAPARSER:
    case CurrentProcessType::PROCESS_SERVICE_QRCODEGENERATOR:
    case CurrentProcessType::PROCESS_SERVICE_PROFILEIMPORT:
    case CurrentProcessType::PROCESS_SERVICE_IME:
    case CurrentProcessType::PROCESS_SERVICE_RECORDING:
    case CurrentProcessType::PROCESS_SERVICE_SHAPEDETECTION:
      return ShortProcessType::kService;
  }
#else
  return ShortProcessType::kUnspecified;
#endif
}

// static
CurrentProcess& CurrentProcess::GetInstance() {
  static base::NoDestructor<CurrentProcess> instance;
  return *instance;
}

void CurrentProcess::SetProcessType(CurrentProcessType process_type) {
  std::string process_name = GetNameForProcessType(process_type);
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
  trace_event::TraceLog::GetInstance()->OnSetProcessName(process_name);
#endif
}

}  // namespace base
