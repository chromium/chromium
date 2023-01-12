// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/plugin_prefs.h"

#include <stddef.h>

#include <string>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/plugins/plugin_prefs_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/common/webplugininfo.h"

using content::BrowserThread;

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
  bool update_internal_dir = false;
  base::FilePath last_internal_dir =
      prefs_->GetFilePath(prefs::kPluginsLastInternalDirectory);
  base::FilePath cur_internal_dir;
  if (base::PathService::Get(chrome::DIR_INTERNAL_PLUGINS, &cur_internal_dir) &&
      cur_internal_dir != last_internal_dir) {
    update_internal_dir = true;
    prefs_->SetFilePath(
        prefs::kPluginsLastInternalDirectory, cur_internal_dir);
  }

  {  // Scoped update of prefs::kPluginsPluginsList.
    ScopedListPrefUpdate update(prefs_, prefs::kPluginsPluginsList);
    base::Value::List& saved_plugins_list = update.Get();
    for (auto& plugin_value : saved_plugins_list) {
      if (!plugin_value.is_dict()) {
        LOG(WARNING) << "Invalid entry in " << prefs::kPluginsPluginsList;
        continue;  // Oops, don't know what to do with this item.
      }
      base::Value::Dict& plugin = plugin_value.GetDict();

      // The plugin list contains all the plugin files in addition to the
      // plugin groups.
      if (const std::string* path = plugin.FindString("path")) {
        // Files have a path attribute, groups don't.
        base::FilePath plugin_path = base::FilePath::FromUTF8Unsafe(*path);

        // The path to the internal plugin directory changes every time Chrome
        // is auto-updated, since it contains the current version number. For
        // example, it changes from foobar\Chrome\Application\21.0.1180.83 to
        // foobar\Chrome\Application\21.0.1180.89.
        // However, we would like the settings of internal plugins to persist
        // across Chrome updates. Therefore, we need to recognize those paths
        // that are within the previous internal plugin directory, and update
        // them in the prefs accordingly.
        if (update_internal_dir) {
          base::FilePath relative_path;

          // Extract the part of |plugin_path| that is relative to
          // |last_internal_dir|. For example, |relative_path| will be
          // foo\bar.dll if |plugin_path| is <last_internal_dir>\foo\bar.dll.
          //
          // Every iteration the last path component from |plugin_path| is
          // removed and prepended to |relative_path| until we get up to
          // |last_internal_dir|.
          while (last_internal_dir.IsParent(plugin_path)) {
            relative_path = plugin_path.BaseName().Append(relative_path);

            base::FilePath old_path = plugin_path;
            plugin_path = plugin_path.DirName();
            // To be extra sure that we won't end up in an infinite loop.
            if (old_path == plugin_path) {
              NOTREACHED();
              break;
            }
          }

          // If |relative_path| is empty, |plugin_path| is not within
          // |last_internal_dir|. We don't need to update it.
          if (!relative_path.empty()) {
            plugin_path = cur_internal_dir.Append(relative_path);
            std::string updated_path = plugin_path.AsUTF8Unsafe();
            plugin.Set("path", updated_path);
          }
        }
      }
    }
  }  // Scoped update of prefs::kPluginsPluginsList.

  UpdatePdfPolicy(prefs::kPluginsAlwaysOpenPdfExternally);
  registrar_.Init(prefs_);
  registrar_.Add(prefs::kPluginsAlwaysOpenPdfExternally,
                 base::BindRepeating(&PluginPrefs::UpdatePdfPolicy,
                                     base::Unretained(this)));
}

void PluginPrefs::ShutdownOnUIThread() {
  prefs_ = nullptr;
  registrar_.RemoveAll();
}

PluginPrefs::PluginPrefs() = default;
PluginPrefs::~PluginPrefs() = default;

void PluginPrefs::SetAlwaysOpenPdfExternallyForTests(
    bool always_open_pdf_externally) {
  always_open_pdf_externally_ = always_open_pdf_externally;
}
