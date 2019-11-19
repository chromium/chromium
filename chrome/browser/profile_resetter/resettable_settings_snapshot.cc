// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profile_resetter/resettable_settings_snapshot.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/guid.h"
#include "base/hash/md5.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/atomic_flag.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task_runner_util.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/profile_resetter/profile_reset_report.pb.h"
#include "chrome/browser/profile_resetter/reset_report_uploader.h"
#include "chrome/browser/profile_resetter/reset_report_uploader_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_registry.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

template <class StringType>
void AddPair(base::ListValue* list,
             const base::string16& key,
             const StringType& value) {
  std::unique_ptr<base::DictionaryValue> results(new base::DictionaryValue());
  results->SetString("key", key);
  results->SetString("value", value);
  list->Append(std::move(results));
}

}  // namespace

ResettableSettingsSnapshot::ResettableSettingsSnapshot(Profile* profile)
    : startup_(SessionStartupPref::GetStartupPref(profile)),
      shortcuts_determined_(false) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // URLs are always stored sorted.
  std::sort(startup_.urls.begin(), startup_.urls.end());

  PrefService* prefs = profile->GetPrefs();
  DCHECK(prefs);
  homepage_ = prefs->GetString(prefs::kHomePage);
  homepage_is_ntp_ = prefs->GetBoolean(prefs::kHomePageIsNewTabPage);
  show_home_button_ = prefs->GetBoolean(prefs::kShowHomeButton);

  TemplateURLService* service =
      TemplateURLServiceFactory::GetForProfile(profile);
  DCHECK(service);
  const TemplateURL* dse = service->GetDefaultSearchProvider();
  if (dse)
    dse_url_ = dse->url();

  const extensions::ExtensionSet& enabled_ext =
      extensions::ExtensionRegistry::Get(profile)->enabled_extensions();
  enabled_extensions_.reserve(enabled_ext.size());

  for (extensions::ExtensionSet::const_iterator it = enabled_ext.begin();
       it != enabled_ext.end(); ++it)
    enabled_extensions_.push_back(std::make_pair((*it)->id(), (*it)->name()));

  // ExtensionSet is sorted but it seems to be an implementation detail.
  std::sort(enabled_extensions_.begin(), enabled_extensions_.end());

  // Calculate the MD5 sum of the GUID to make sure that no part of the GUID
  // contains information identifying the sender of the report.
  guid_ = base::MD5String(base::GenerateGUID());
}

ResettableSettingsSnapshot::~ResettableSettingsSnapshot() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (cancellation_flag_.get())
    cancellation_flag_->data.Set();
}

void ResettableSettingsSnapshot::Subtract(
    const ResettableSettingsSnapshot& snapshot) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  ExtensionList extensions = base::STLSetDifference<ExtensionList>(
      enabled_extensions_, snapshot.enabled_extensions_);
  enabled_extensions_.swap(extensions);
}

int ResettableSettingsSnapshot::FindDifferentFields(
    const ResettableSettingsSnapshot& snapshot) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  int bit_mask = 0;

  if (startup_.type != snapshot.startup_.type ||
      startup_.urls != snapshot.startup_.urls)
    bit_mask |= STARTUP_MODE;

  if (homepage_is_ntp_ != snapshot.homepage_is_ntp_ ||
      homepage_ != snapshot.homepage_ ||
      show_home_button_ != snapshot.show_home_button_)
    bit_mask |= HOMEPAGE;

  if (dse_url_ != snapshot.dse_url_)
    bit_mask |= DSE_URL;

  if (enabled_extensions_ != snapshot.enabled_extensions_)
    bit_mask |= EXTENSIONS;

  if (shortcuts_ != snapshot.shortcuts_)
    bit_mask |= SHORTCUTS;

  static_assert(ResettableSettingsSnapshot::ALL_FIELDS == 31,
                "new field needs to be added here");

  return bit_mask;
}

void ResettableSettingsSnapshot::RequestShortcuts(
    const base::Closure& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!cancellation_flag_.get() && !shortcuts_determined());

  cancellation_flag_ = new SharedCancellationFlag;
#if defined(OS_WIN)
  base::PostTaskAndReplyWithResult(
      base::CreateCOMSTATaskRunner({base::ThreadPool(), base::MayBlock(),
                                    base::TaskPriority::USER_VISIBLE})
          .get(),
      FROM_HERE, base::Bind(&GetChromeLaunchShortcuts, cancellation_flag_),
      base::Bind(&ResettableSettingsSnapshot::SetShortcutsAndReport,
                 weak_ptr_factory_.GetWeakPtr(), callback));
#else   // defined(OS_WIN)
  // Shortcuts are only supported on Windows.
  std::vector<ShortcutCommand> no_shortcuts;
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&ResettableSettingsSnapshot::SetShortcutsAndReport,
                     weak_ptr_factory_.GetWeakPtr(), callback,
                     std::move(no_shortcuts)));
#endif  // defined(OS_WIN)
}

void ResettableSettingsSnapshot::SetShortcutsAndReport(
    const base::Closure& callback,
    const std::vector<ShortcutCommand>& shortcuts) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  shortcuts_ = shortcuts;
  shortcuts_determined_ = true;
  cancellation_flag_.reset();

  if (!callback.is_null())
    callback.Run();
}

std::unique_ptr<reset_report::ChromeResetReport> SerializeSettingsReportToProto(
    const ResettableSettingsSnapshot& snapshot,
    int field_mask) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::unique_ptr<reset_report::ChromeResetReport> report(
      new reset_report::ChromeResetReport());

  if (field_mask & ResettableSettingsSnapshot::STARTUP_MODE) {
    for (const auto& url : snapshot.startup_urls())
      report->add_startup_url_path(url.spec());
    switch (snapshot.startup_type()) {
      case SessionStartupPref::DEFAULT:
        report->set_startup_type(
            reset_report::ChromeResetReport_SessionStartupType_DEFAULT);
        break;
      case SessionStartupPref::LAST:
        report->set_startup_type(
            reset_report::ChromeResetReport_SessionStartupType_LAST);
        break;
      case SessionStartupPref::URLS:
        report->set_startup_type(
            reset_report::ChromeResetReport_SessionStartupType_URLS);
        break;
    }
  }

  if (field_mask & ResettableSettingsSnapshot::HOMEPAGE) {
    report->set_homepage_path(snapshot.homepage());
    report->set_homepage_is_new_tab_page(snapshot.homepage_is_ntp());
    report->set_show_home_button(snapshot.show_home_button());
  }

  if (field_mask & ResettableSettingsSnapshot::DSE_URL)
    report->set_default_search_engine_path(snapshot.dse_url());

  if (field_mask & ResettableSettingsSnapshot::EXTENSIONS) {
    for (const auto& enabled_extension : snapshot.enabled_extensions()) {
      reset_report::ChromeResetReport_Extension* new_extension =
          report->add_enabled_extensions();
      new_extension->set_extension_id(enabled_extension.first);
      new_extension->set_extension_name(enabled_extension.second);
    }
  }

  if (field_mask & ResettableSettingsSnapshot::SHORTCUTS) {
    for (const auto& shortcut_command : snapshot.shortcuts())
      report->add_shortcuts(base::UTF16ToUTF8(shortcut_command.second));
  }

  report->set_guid(snapshot.guid());

  static_assert(ResettableSettingsSnapshot::ALL_FIELDS == 31,
                "new field needs to be serialized here");
  return report;
}

void SendSettingsFeedbackProto(const reset_report::ChromeResetReport& report,
                               Profile* profile) {
  ResetReportUploaderFactory::GetForBrowserContext(profile)
      ->DispatchReport(report);
}

std::unique_ptr<base::ListValue> GetReadableFeedbackForSnapshot(
    Profile* profile,
    const ResettableSettingsSnapshot& snapshot) {
  DCHECK(profile);
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::unique_ptr<base::ListValue> list(new base::ListValue);
  AddPair(list.get(),
          l10n_util::GetStringUTF16(IDS_RESET_PROFILE_SETTINGS_LOCALE),
          g_browser_process->GetApplicationLocale());
  AddPair(list.get(),
          l10n_util::GetStringUTF16(IDS_VERSION_UI_USER_AGENT),
          GetUserAgent());
  std::string version = version_info::GetVersionNumber();
  version += chrome::GetChannelName();
  AddPair(list.get(),
          l10n_util::GetStringUTF16(IDS_PRODUCT_NAME),
          version);

  // Add snapshot data.
  const std::vector<GURL>& urls = snapshot.startup_urls();
  std::string startup_urls;
  for (auto i = urls.begin(); i != urls.end(); ++i) {
    if (!startup_urls.empty())
      startup_urls += ' ';
    startup_urls += i->host();
  }
  if (!startup_urls.empty()) {
    AddPair(list.get(),
            l10n_util::GetStringUTF16(IDS_RESET_PROFILE_SETTINGS_STARTUP_URLS),
            startup_urls);
  }

  base::string16 startup_type;
  switch (snapshot.startup_type()) {
    case SessionStartupPref::DEFAULT:
      startup_type =
          l10n_util::GetStringUTF16(IDS_SETTINGS_ON_STARTUP_OPEN_NEW_TAB);
      break;
    case SessionStartupPref::LAST:
      startup_type =
          l10n_util::GetStringUTF16(IDS_SETTINGS_ON_STARTUP_CONTINUE);
      break;
    case SessionStartupPref::URLS:
      startup_type =
          l10n_util::GetStringUTF16(IDS_SETTINGS_ON_STARTUP_OPEN_SPECIFIC);
      break;
    default:
      break;
  }
  AddPair(list.get(),
          l10n_util::GetStringUTF16(IDS_RESET_PROFILE_SETTINGS_STARTUP_TYPE),
          startup_type);

  if (!snapshot.homepage().empty()) {
    AddPair(list.get(),
            l10n_util::GetStringUTF16(IDS_RESET_PROFILE_SETTINGS_HOMEPAGE),
            snapshot.homepage());
  }

  int is_ntp_message_id = snapshot.homepage_is_ntp()
      ? IDS_RESET_PROFILE_SETTINGS_YES
      : IDS_RESET_PROFILE_SETTINGS_NO;
  AddPair(list.get(),
          l10n_util::GetStringUTF16(IDS_RESET_PROFILE_SETTINGS_HOMEPAGE_IS_NTP),
          l10n_util::GetStringUTF16(is_ntp_message_id));

  int show_home_button_id = snapshot.show_home_button()
      ? IDS_RESET_PROFILE_SETTINGS_YES
      : IDS_RESET_PROFILE_SETTINGS_NO;
  AddPair(
      list.get(),
      l10n_util::GetStringUTF16(IDS_RESET_PROFILE_SETTINGS_SHOW_HOME_BUTTON),
      l10n_util::GetStringUTF16(show_home_button_id));

  TemplateURLService* service =
      TemplateURLServiceFactory::GetForProfile(profile);
  DCHECK(service);
  const TemplateURL* dse = service->GetDefaultSearchProvider();
  if (dse) {
    AddPair(list.get(),
            l10n_util::GetStringUTF16(IDS_RESET_PROFILE_SETTINGS_DSE),
            dse->GenerateSearchURL(service->search_terms_data()).host());
  }

  if (snapshot.shortcuts_determined()) {
    base::string16 shortcut_targets;
    const std::vector<ShortcutCommand>& shortcuts = snapshot.shortcuts();
    for (auto i = shortcuts.begin(); i != shortcuts.end(); ++i) {
      if (!shortcut_targets.empty())
        shortcut_targets += base::ASCIIToUTF16("\n");
      shortcut_targets += base::ASCIIToUTF16("chrome.exe ");
      shortcut_targets += i->second;
    }
    if (!shortcut_targets.empty()) {
      AddPair(list.get(),
              l10n_util::GetStringUTF16(IDS_RESET_PROFILE_SETTINGS_SHORTCUTS),
              shortcut_targets);
    }
  } else {
    AddPair(list.get(),
            l10n_util::GetStringUTF16(IDS_RESET_PROFILE_SETTINGS_SHORTCUTS),
            l10n_util::GetStringUTF16(
                IDS_RESET_PROFILE_SETTINGS_PROCESSING_SHORTCUTS));
  }

  const ResettableSettingsSnapshot::ExtensionList& extensions =
      snapshot.enabled_extensions();
  std::string extension_names;
  for (auto i = extensions.begin(); i != extensions.end(); ++i) {
    if (!extension_names.empty())
      extension_names += '\n';
    extension_names += i->second;
  }
  if (!extension_names.empty()) {
    AddPair(list.get(),
            l10n_util::GetStringUTF16(IDS_RESET_PROFILE_SETTINGS_EXTENSIONS),
            extension_names);
  }
  return list;
}
