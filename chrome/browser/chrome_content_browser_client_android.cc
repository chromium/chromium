// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_content_browser_client_android.h"

#include <vector>

#include "chrome/common/chrome_descriptors_android.h"
#include "content/public/browser/posix_file_descriptor_info.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/resource/resource_bundle_android.h"

// Helper function for GetAdditionalMappedFilesForChildProcess().
void GetMappedLocalePacksForChildProcess(
    content::PosixFileDescriptorInfo* mappings) {
  const std::vector<ui::ResourceBundle::FdAndRegion>& locale_paks =
      ui::GetLocalePaks();
  // We can have up to 4 locale paks here: WebView-gendered,
  // non-WebView-gendered, WebView-fallback, and non-WebView-fallback. The
  // "fallback" paks are for the default gender, and will be read from if a
  // particular string doesn't exist in the corresponding gendered pak.
  CHECK_GE(locale_paks.size(), 1u);
  CHECK_LE(locale_paks.size(), 4u);

  for (auto& pak : locale_paks) {
    int descriptor;
    switch (pak.purpose) {
      case ui::ResourceBundle::LocalePakPurpose::kWebViewMain:
        descriptor = kAndroidMainWebViewLocalePakDescriptor;
        break;
      case ui::ResourceBundle::LocalePakPurpose::kNonWebViewMain:
        descriptor = kAndroidMainNonWebViewLocalePakDescriptor;
        break;
      case ui::ResourceBundle::LocalePakPurpose::kWebViewFallback:
        descriptor = kAndroidFallbackWebViewLocalePakDescriptor;
        break;
      case ui::ResourceBundle::LocalePakPurpose::kNonWebViewFallback:
        descriptor = kAndroidFallbackNonWebViewLocalePakDescriptor;
        break;
      default:
        CHECK(false);
    }
    CHECK_GE(pak.fd, 0);
    mappings->ShareWithRegion(descriptor, pak.fd, pak.region);
  }
}
