// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/soda_installer_impl.h"

#include <map>
#include <string>

#include "base/bind.h"
#include "base/check_op.h"
#include "base/no_destructor.h"
#include "base/numerics/ranges.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/soda_component_installer.h"
#include "chrome/browser/component_updater/soda_en_us_component_installer.h"
#include "chrome/browser/component_updater/soda_ja_jp_component_installer.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/update_client/crx_update_item.h"

namespace {

int GetDownloadProgress(
    const std::map<std::string, update_client::CrxUpdateItem>&
        downloading_components) {
  int total_bytes = 0;
  int downloaded_bytes = 0;

  for (auto component : downloading_components) {
    if (component.second.downloaded_bytes >= 0 &&
        component.second.total_bytes > 0) {
      downloaded_bytes += component.second.downloaded_bytes;
      total_bytes += component.second.total_bytes;
    }
  }

  if (total_bytes == 0)
    return -1;

  DCHECK_LE(downloaded_bytes, total_bytes);
  return 100 *
         base::ClampToRange(double{downloaded_bytes} / total_bytes, 0.0, 1.0);
}

}  // namespace

namespace speech {

// static
SODAInstaller* SODAInstaller::GetInstance() {
  static base::NoDestructor<SODAInstallerImpl> instance;
  return instance.get();
}

SODAInstallerImpl::SODAInstallerImpl() = default;

SODAInstallerImpl::~SODAInstallerImpl() {
  component_updater_observer_.RemoveAll();
}

void SODAInstallerImpl::InstallSODA(PrefService* prefs) {
  component_updater::RegisterSodaComponent(
      g_browser_process->component_updater(), prefs,
      g_browser_process->local_state(),
      base::BindOnce(&component_updater::SODAComponentInstallerPolicy::
                         UpdateSODAComponentOnDemand));

  if (!component_updater_observer_.IsObserving(
          g_browser_process->component_updater())) {
    component_updater_observer_.Add(g_browser_process->component_updater());
  }
}

void SODAInstallerImpl::InstallLanguage(PrefService* prefs) {
  component_updater::RegisterSodaLanguageComponent(
      g_browser_process->component_updater(), prefs,
      g_browser_process->local_state());

  if (!component_updater_observer_.IsObserving(
          g_browser_process->component_updater())) {
    component_updater_observer_.Add(g_browser_process->component_updater());
  }
}

void SODAInstallerImpl::OnEvent(Events event, const std::string& id) {
  if (id != component_updater::SODAComponentInstallerPolicy::GetExtensionId() &&
      id != component_updater::SodaEnUsComponentInstallerPolicy::
                GetExtensionId() &&
      id !=
          component_updater::SodaJaJpComponentInstallerPolicy::GetExtensionId())
    return;

  switch (event) {
    case Events::COMPONENT_UPDATE_FOUND:
    case Events::COMPONENT_UPDATE_READY:
    case Events::COMPONENT_WAIT:
    case Events::COMPONENT_UPDATE_DOWNLOADING:
    case Events::COMPONENT_UPDATE_UPDATING: {
      update_client::CrxUpdateItem item;
      g_browser_process->component_updater()->GetComponentDetails(id, &item);
      downloading_components_[id] = item;
      const int progress = GetDownloadProgress(downloading_components_);
      // When GetDownloadProgress returns -1, do nothing. It returns -1 when the
      // downloaded or total bytes is unknown.
      if (progress != -1) {
        NotifyOnSODAProgress(progress);
      }
    } break;
    case Events::COMPONENT_UPDATED:
    case Events::COMPONENT_NOT_UPDATED:
      NotifyOnSODAInstalled();
      break;
    case Events::COMPONENT_UPDATE_ERROR:
      NotifyOnSODAError();
      break;
    case Events::COMPONENT_CHECKING_FOR_UPDATES:
      // Do nothing.
      break;
  }
}

}  // namespace speech
