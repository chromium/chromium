// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/plugin_prefs.h"

#include <stddef.h>

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "chrome/browser/plugins/plugin_prefs_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/common/webplugininfo.h"

namespace {

bool IsPDFViewerPlugin(const content::WebPluginInfo& plugin) {
  // This should only match the external PDF plugin, not the internal PDF
  // plugin, which is also used for Print Preview. Note that only the PDF viewer
  // and Print Preview can create the internal PDF plugin in the first place.
  return plugin.path.value() == ChromeContentClient::kPDFExtensionPluginPath;
}

}  // namespace

// static
scoped_refptr<PluginPrefs> PluginPrefs::GetForProfile(Profile* profile) {
  return PluginPrefsFactory::GetPrefsForProfile(profile);
}

// static
scoped_refptr<PluginPrefs> PluginPrefs::GetForTestingProfile(
    Profile* profile) {
  return static_cast<PluginPrefs*>(
      PluginPrefsFactory::GetInstance()
          ->SetTestingFactoryAndUse(
              profile,
              base::BindRepeating(&PluginPrefsFactory::CreateForTestingProfile))
          .get());
}

bool PluginPrefs::IsPluginEnabled(const content::WebPluginInfo& plugin) const {
  return !IsPDFViewerPlugin(plugin) || !always_open_pdf_externally_;
}

void PluginPrefs::UpdatePdfPolicy(const std::string& pref_name) {
  always_open_pdf_externally_ =
      prefs_->GetBoolean(prefs::kPluginsAlwaysOpenPdfExternally);

  content::PluginService::GetInstance()->PurgePluginListCache(profile_, false);
  std::vector<Profile*> otr_profiles = profile_->GetAllOffTheRecordProfiles();
  for (Profile* otr : otr_profiles)
    content::PluginService::GetInstance()->PurgePluginListCache(otr, false);
}

void PluginPrefs::SetPrefs(PrefService* prefs) {
  prefs_ = prefs;

  UpdatePdfPolicy(prefs::kPluginsAlwaysOpenPdfExternally);
  registrar_ = std::make_unique<PrefChangeRegistrar>();
  registrar_->Init(prefs_);
  registrar_->Add(prefs::kPluginsAlwaysOpenPdfExternally,
                  base::BindRepeating(&PluginPrefs::UpdatePdfPolicy,
                                      base::Unretained(this)));
}

void PluginPrefs::ShutdownOnUIThread() {
  prefs_ = nullptr;
  registrar_.reset();
  profile_ = nullptr;
}

PluginPrefs::PluginPrefs() = default;
PluginPrefs::~PluginPrefs() = default;

void PluginPrefs::SetAlwaysOpenPdfExternallyForTests(
    bool always_open_pdf_externally) {
  always_open_pdf_externally_ = always_open_pdf_externally;
}
