// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/accessibility/soda_installer_impl_chromeos.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "chromeos/dbus/dlcservice/dlcservice_client.h"
#include "media/base/media_switches.h"

namespace {

// TODO(crbug.com/1111002): Replace this with the real SODA DLC id.
constexpr char kSODADlcName[] = "soda";

}  // namespace

namespace speech {

SODAInstaller* SODAInstaller::GetInstance() {
  static base::NoDestructor<SODAInstallerImplChromeOS> instance;
  return instance.get();
}

SODAInstallerImplChromeOS::SODAInstallerImplChromeOS() = default;

SODAInstallerImplChromeOS::~SODAInstallerImplChromeOS() = default;

void SODAInstallerImplChromeOS::InstallSODA(PrefService* prefs) {
  if (!base::FeatureList::IsEnabled(media::kUseSodaForLiveCaption))
    return;

  // Install SODA DLC.
  chromeos::DlcserviceClient::Get()->Install(
      kSODADlcName,
      base::BindOnce(&SODAInstallerImplChromeOS::OnSODAInstalled,
                     base::Unretained(this)),
      base::BindRepeating(&SODAInstallerImplChromeOS::OnSODAProgress,
                          base::Unretained(this)));
}

void SODAInstallerImplChromeOS::InstallLanguage(PrefService* prefs) {
  // TODO(crbug.com/1111002): Install SODA language.
}

void SODAInstallerImplChromeOS::OnSODAInstalled(
    const chromeos::DlcserviceClient::InstallResult& install_result) {
  if (install_result.error == dlcservice::kErrorNone) {
    NotifyOnSODAInstalled();
  } else {
    NotifyOnSODAError();
  }
}

void SODAInstallerImplChromeOS::OnSODAProgress(double progress) {
  NotifyOnSODAProgress(int{100 * progress});
}

}  // namespace speech
