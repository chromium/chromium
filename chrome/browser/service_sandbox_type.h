// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SERVICE_SANDBOX_TYPE_H_
#define CHROME_BROWSER_SERVICE_SANDBOX_TYPE_H_

#include "build/build_config.h"
#include "content/public/browser/service_process_host.h"
#include "media/base/media_switches.h"
#include "sandbox/policy/sandbox_type.h"

// This file maps service classes to sandbox types.  Services which
// require a non-utility sandbox can be added here.  See
// ServiceProcessHost::Launch() for how these templates are consumed.

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
  if (base::FeatureList::IsEnabled(media::kUseSodaForLiveCaption)) {
    return sandbox::policy::SandboxType::kSpeechRecognition;
  } else {
    return sandbox::policy::SandboxType::kUtility;
  }
}
#endif  // !defined(OS_ANDROID)

// printing::mojom::PrintingService
#if defined(OS_WIN)
namespace printing {
namespace mojom {
class PrintingService;
}
}  // namespace printing

template <>
inline sandbox::policy::SandboxType
content::GetServiceSandboxType<printing::mojom::PrintingService>() {
  return sandbox::policy::SandboxType::kPdfConversion;
}
#endif  // defined(OS_WIN)

// proxy_resolver::mojom::ProxyResolverFactory
#if defined(OS_WIN)
namespace proxy_resolver {
namespace mojom {
class ProxyResolverFactory;
}
}  // namespace proxy_resolver

template <>
inline sandbox::policy::SandboxType
content::GetServiceSandboxType<proxy_resolver::mojom::ProxyResolverFactory>() {
  return sandbox::policy::SandboxType::kProxyResolver;
}
#endif  // defined(OS_WIN)

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
  return sandbox::policy::SandboxType::kSharingService;
}
#endif  // !defined(OS_MAC)

#endif  // CHROME_BROWSER_SERVICE_SANDBOX_TYPE_H_
