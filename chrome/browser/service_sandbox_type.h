// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SERVICE_SANDBOX_TYPE_H_
#define CHROME_BROWSER_SERVICE_SANDBOX_TYPE_H_

#include "build/build_config.h"
#include "content/public/browser/service_process_host.h"
#include "media/base/media_switches.h"
#include "printing/buildflags/buildflags.h"
#include "sandbox/policy/sandbox_type.h"

#if (defined(OS_WIN) || defined(OS_MAC) || defined(OS_LINUX) || \
     defined(OS_CHROMEOS)) &&                                   \
    BUILDFLAG(ENABLE_PRINTING)
#include "chrome/browser/printing/print_backend_service_manager.h"
#endif

// This file maps service classes to sandbox types. See
// ServiceProcessHost::Launch() for how these templates are consumed.

// chrome::mojom::FileUtilService
namespace chrome {
namespace mojom {
class FileUtilService;
}
}  // namespace chrome
template <>
inline sandbox::policy::SandboxType
content::GetServiceSandboxType<chrome::mojom::FileUtilService>() {
  return sandbox::policy::SandboxType::kUtility;
}

// chrome::mojom::RemovableStorageWriter
namespace chrome {
namespace mojom {
class RemovableStorageWriter;
}  // namespace mojom
}  // namespace chrome

template <>
inline sandbox::policy::SandboxType
content::GetServiceSandboxType<chrome::mojom::RemovableStorageWriter>() {
#if defined(OS_WIN)
  return sandbox::policy::SandboxType::kNoSandboxAndElevatedPrivileges;
#else
  return sandbox::policy::SandboxType::kNoSandbox;
#endif  // !defined(OS_WIN)
}

// chrome::mojom::UtilReadIcon
#if defined(OS_WIN)
namespace chrome {
namespace mojom {
class UtilReadIcon;
}
}  // namespace chrome

template <>
inline sandbox::policy::SandboxType
content::GetServiceSandboxType<chrome::mojom::UtilReadIcon>() {
  return sandbox::policy::SandboxType::kIconReader;
}
#endif  // defined(OS_WIN)

// chrome::mojom::UtilWin
#if defined(OS_WIN)
namespace chrome {
namespace mojom {
class UtilWin;
}
}  // namespace chrome

template <>
inline sandbox::policy::SandboxType
content::GetServiceSandboxType<chrome::mojom::UtilWin>() {
  return sandbox::policy::SandboxType::kNoSandbox;
}
#endif  // defined(OS_WIN)

// chrome::mojom::ProcessorMetrics
#if defined(OS_WIN)
namespace chrome {
namespace mojom {
class ProcessorMetrics;
}
}  // namespace chrome

template <>
inline sandbox::policy::SandboxType
content::GetServiceSandboxType<chrome::mojom::ProcessorMetrics>() {
  return sandbox::policy::SandboxType::kNoSandbox;
}
#endif  // defined(OS_WIN)

// chrome::mojom::ProfileImport
namespace chrome {
namespace mojom {
class ProfileImport;
}
}  // namespace chrome

template <>
inline sandbox::policy::SandboxType
content::GetServiceSandboxType<chrome::mojom::ProfileImport>() {
  return sandbox::policy::SandboxType::kNoSandbox;
}

// mac_notifications::mojom::MacNotificationProvider
#if defined(OS_MAC)
namespace mac_notifications {
namespace mojom {
class MacNotificationProvider;
}  // namespace mojom
}  // namespace mac_notifications

template <>
inline sandbox::policy::SandboxType content::GetServiceSandboxType<
    mac_notifications::mojom::MacNotificationProvider>() {
  return sandbox::policy::SandboxType::kNoSandbox;
}
#endif  // defined(OS_MAC)

// media::mojom::SpeechRecognitionService
#if !defined(OS_ANDROID)
namespace media {
namespace mojom {
class SpeechRecognitionService;
}
}  // namespace media

template <>
inline sandbox::policy::SandboxType
content::GetServiceSandboxType<media::mojom::SpeechRecognitionService>() {
  return sandbox::policy::SandboxType::kSpeechRecognition;
}
#endif  // !defined(OS_ANDROID)

// mirroring::mojom::MirroringService
namespace mirroring {
namespace mojom {
class MirroringService;
}
}  // namespace mirroring
template <>
inline sandbox::policy::SandboxType
content::GetServiceSandboxType<mirroring::mojom::MirroringService>() {
#if defined(OS_MAC)
  return sandbox::policy::SandboxType::kMirroring;
#else
  return sandbox::policy::SandboxType::kUtility;
#endif  // OS_MAC
}

// printing::mojom::PrintingService
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
namespace printing {
namespace mojom {
class PrintingService;
}
}  // namespace printing

template <>
inline sandbox::policy::SandboxType
content::GetServiceSandboxType<printing::mojom::PrintingService>() {
#if defined(OS_WIN)
  return sandbox::policy::SandboxType::kPdfConversion;
#else
  return sandbox::policy::SandboxType::kUtility;
#endif
}
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

// printing::mojom::PrintBackendService
#if (defined(OS_WIN) || defined(OS_MAC) || defined(OS_LINUX) || \
     defined(OS_CHROMEOS)) &&                                   \
    BUILDFLAG(ENABLE_PRINTING)
namespace printing {
namespace mojom {
class PrintBackendService;
}
}  // namespace printing

template <>
inline sandbox::policy::SandboxType
content::GetServiceSandboxType<printing::mojom::PrintBackendService>() {
  return printing::PrintBackendServiceManager::GetInstance()
                 .ShouldSandboxPrintBackendService()
             ? sandbox::policy::SandboxType::kPrintBackend
             : sandbox::policy::SandboxType::kNoSandbox;
}
#endif  // (defined(OS_WIN) || defined(OS_MAC) || defined(OS_LINUX) ||
        //  defined(OS_CHROMEOS)) &&
        // BUILDFLAG(ENABLE_PRINTING)

// proxy_resolver::mojom::ProxyResolverFactory
#if !defined(OS_ANDROID)
namespace proxy_resolver {
namespace mojom {
class ProxyResolverFactory;
}
}  // namespace proxy_resolver

template <>
inline sandbox::policy::SandboxType
content::GetServiceSandboxType<proxy_resolver::mojom::ProxyResolverFactory>() {
#if defined(OS_WIN)
  return sandbox::policy::SandboxType::kProxyResolver;
#else  // (!OS_WIN && !OS_ANDROID)
  return sandbox::policy::SandboxType::kUtility;
#endif
}
#endif  // !defined(OS_ANDROID)

// quarantine::mojom::Quarantine
#if defined(OS_WIN)
namespace quarantine {
namespace mojom {
class Quarantine;
}
}  // namespace quarantine

template <>
inline sandbox::policy::SandboxType
content::GetServiceSandboxType<quarantine::mojom::Quarantine>() {
  return sandbox::policy::SandboxType::kNoSandbox;
}
#endif  // defined(OS_WIN)

// sharing::mojom::Sharing
#if !defined(OS_MAC)
namespace sharing {
namespace mojom {
class Sharing;
}
}  // namespace sharing

template <>
inline sandbox::policy::SandboxType
content::GetServiceSandboxType<sharing::mojom::Sharing>() {
  return sandbox::policy::SandboxType::kService;
}
#endif  // !defined(OS_MAC)

#endif  // CHROME_BROWSER_SERVICE_SANDBOX_TYPE_H_
