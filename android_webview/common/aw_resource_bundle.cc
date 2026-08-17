// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/common/aw_resource_bundle.h"

#include "android_webview/common/aw_descriptors.h"
#include "base/android/locale_utils.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/memory_mapped_file.h"
#include "base/i18n/icubridge/default_icu_locale.h"
#include "base/i18n/language_tag.h"
#include "base/i18n/tag_converters.h"
#include "base/logging.h"
#include "base/posix/global_descriptors.h"
#include "base/trace_event/trace_event.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/resource/resource_bundle_android.h"

namespace android_webview {

using ::base::i18n::GetKnownLanguageTag;
using ::base::i18n::GetLanguageTagFromString;
using ::base::i18n::LanguageTag;
using ::base::i18n::SetDefaultIcuLocale;

void InitIcuAndResourceBundleBrowserSide() {
  TRACE_EVENT0("startup", "InitIcuAndResourceBundleBrowserSide");
  ui::SetLocalePaksStoredInApk(true);
  std::string locale_string = ui::ResourceBundle::InitSharedInstanceWithLocale(
      base::android::GetDefaultLocaleString(), nullptr,
      ui::ResourceBundle::LOAD_COMMON_RESOURCES);
  std::optional<LanguageTag> locale_tag =
      GetLanguageTagFromString(locale_string);
  if (!locale_tag) {
    LOG(WARNING) << "Failed to load locale .pak from apk.";
  }
  SetDefaultIcuLocale(base::i18n::DefaultIcuLocaleSetterKey(),
                      locale_tag.value_or(GetKnownLanguageTag("en-US")));

  // We only load the resources.pak from the apk.
  ui::LoadMainAndroidPackFile("assets/resources.pak", base::FilePath());
}

void InitResourceBundleRendererSide() {
  auto* global_descriptors = base::GlobalDescriptors::GetInstance();
  int pak_fd = global_descriptors->Get(kAndroidWebViewLocalePakDescriptor);
  base::MemoryMappedFile::Region pak_region =
      global_descriptors->GetRegion(kAndroidWebViewLocalePakDescriptor);
  ui::ResourceBundle::InitSharedInstanceWithPakFileRegion(base::File(pak_fd),
                                                          pak_region);

  std::pair<int, ui::ResourceScaleFactor> extra_paks[] = {
      {kAndroidWebViewMainPakDescriptor, ui::kScaleFactorNone},
      {kAndroidWebView100PercentPakDescriptor, ui::k100Percent}};

  for (const auto& pak_info : extra_paks) {
    pak_fd = global_descriptors->Get(pak_info.first);
    pak_region = global_descriptors->GetRegion(pak_info.first);
    ui::ResourceBundle::GetSharedInstance().AddDataPackFromFileRegion(
        base::File(pak_fd), pak_region, pak_info.second);
  }
}

}  // namespace android_webview
