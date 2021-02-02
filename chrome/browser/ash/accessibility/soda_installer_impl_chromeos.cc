// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/soda_installer_impl_chromeos.h"

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

  has_soda_ = false;
  // Clear cached path in case this is a reinstallation (path could
  // change).
  SetSodaLibPath(base::FilePath());

  // Install SODA DLC.
  chromeos::DlcserviceClient::Get()->Install(
      kSodaDlcName,
      base::BindOnce(&SodaInstallerImplChromeOS::OnSodaInstalled,
                     base::Unretained(this)),
      base::BindRepeating(&SodaInstallerImplChromeOS::OnSodaProgress,
                          base::Unretained(this)));
}

base::FilePath SodaInstallerImplChromeOS::GetSodaLibPath() const {
  return soda_lib_path_;
}

void SodaInstallerImplChromeOS::InstallLanguage(PrefService* prefs) {
  // TODO(crbug.com/1111002): Install SODA language.
}

bool SodaInstallerImplChromeOS::IsSodaInstalled() const {
  DCHECK(base::FeatureList::IsEnabled(media::kUseSodaForLiveCaption));
  // TODO(crbug.com/1111002): Return whether SODA is installed.
  return has_soda_;
}

void SodaInstallerImplChromeOS::SetSodaLibPath(base::FilePath new_path) {
  soda_lib_path_ = new_path;
}

void SodaInstallerImplChromeOS::OnSodaInstalled(
    const chromeos::DlcserviceClient::InstallResult& install_result) {
  if (install_result.error == dlcservice::kErrorNone) {
    has_soda_ = true;
    SetSodaLibPath(base::FilePath(install_result.root_path));
    NotifyOnSodaInstalled();
  } else {
    NotifyOnSodaError();
  }
}

void SodaInstallerImplChromeOS::OnSodaProgress(double progress) {
  NotifyOnSodaProgress(int{100 * progress});
}

}  // namespace speech
