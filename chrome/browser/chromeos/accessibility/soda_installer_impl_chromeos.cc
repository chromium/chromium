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
constexpr char kSodaDlcName[] = "soda";

}  // namespace

namespace speech {

SodaInstaller* SodaInstaller::GetInstance() {
  static base::NoDestructor<SodaInstallerImplChromeOS> instance;
  return instance.get();
}

SodaInstallerImplChromeOS::SodaInstallerImplChromeOS() = default;

SodaInstallerImplChromeOS::~SodaInstallerImplChromeOS() = default;

void SodaInstallerImplChromeOS::InstallSoda(PrefService* prefs) {
  if (!base::FeatureList::IsEnabled(media::kUseSodaForLiveCaption))
    return;

  // Install SODA DLC.
  chromeos::DlcserviceClient::Get()->Install(
      kSodaDlcName,
      base::BindOnce(&SodaInstallerImplChromeOS::OnSodaInstaller,
                     base::Unretained(this)),
      base::BindRepeating(&SodaInstallerImplChromeOS::OnSodaProgress,
                          base::Unretained(this)));
}

void SodaInstallerImplChromeOS::InstallLanguage(PrefService* prefs) {
  // TODO(crbug.com/1111002): Install SODA language.
}

bool SodaInstallerImplChromeOS::IsSodaRegistered() {
  // TODO(crbug.com/1111002): Return whether SODA is registered.
  return !base::FeatureList::IsEnabled(media::kUseSodaForLiveCaption);
}

void SodaInstallerImplChromeOS::OnSodaInstaller(
    const chromeos::DlcserviceClient::InstallResult& install_result) {
  if (install_result.error == dlcservice::kErrorNone) {
    NotifyOnSodaInstaller();
  } else {
    NotifyOnSodaError();
  }
}

void SodaInstallerImplChromeOS::OnSodaProgress(double progress) {
  NotifyOnSodaProgress(int{100 * progress});
}

}  // namespace speech
