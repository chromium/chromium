// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/error_console/error_console.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/lazy_instance.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/error_console/error_console_factory.h"
#include "chrome/common/pref_names.h"
#include "components/crx_file/id_util.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/features/feature_channel.h"

namespace extensions {

namespace {

// The key into the Extension prefs for an Extension's specific reporting
// settings.
const char kStoreExtensionErrorsPref[] = "store_extension_errors";

// This is the default mask for which errors to report. That is, if an extension
// does not have specific preference set, this will be used instead.
const int kDefaultMask = 0;

const char kAppsDeveloperToolsExtensionId[] =
    "ohmmkhmmmpcnpikjeljgnaoabkaalbgc";

}  // namespace

void ErrorConsole::Observer::OnErrorAdded(const ExtensionError* error) {
}

void ErrorConsole::Observer::OnErrorsRemoved(
    const std::set<std::string>& extension_ids) {
}

void ErrorConsole::Observer::OnErrorConsoleDestroyed() {
}

ErrorConsole::ErrorConsole(Profile* profile)
    : enabled_(false),
      default_mask_(kDefaultMask),
      profile_(profile),
      prefs_(nullptr) {
  pref_registrar_.Init(profile_->GetPrefs());
  pref_registrar_.Add(prefs::kExtensionsUIDeveloperMode,
                      base::Bind(&ErrorConsole::OnPrefChanged,
                                 base::Unretained(this)));

  registry_observer_.Add(ExtensionRegistry::Get(profile_));

  CheckEnabled();
}

ErrorConsole::~ErrorConsole() {
  for (auto& observer : observers_)
    observer.OnErrorConsoleDestroyed();
}

// static
ErrorConsole* ErrorConsole::Get(content::BrowserContext* browser_context) {
  return ErrorConsoleFactory::GetForBrowserContext(browser_context);
}

void ErrorConsole::SetReportingForExtension(const std::string& extension_id,
                                            ExtensionError::Type type,
                                            bool enabled) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!enabled_ || !crx_file::id_util::IdIsValid(extension_id))
    return;

  int mask = default_mask_;
  // This call can fail if the preference isn't set, but we don't really care
  // if it does, because we just use the default mask instead.
  prefs_->ReadPrefAsInteger(extension_id, kStoreExtensionErrorsPref, &mask);

  if (enabled)
    mask |= 1 << type;
  else
    mask &= ~(1 << type);

  prefs_->UpdateExtensionPref(extension_id, kStoreExtensionErrorsPref,
                              std::make_unique<base::Value>(mask));
}

void ErrorConsole::SetReportingAllForExtension(
    const std::string& extension_id, bool enabled) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!enabled_ || !crx_file::id_util::IdIsValid(extension_id))
    return;

  int mask = enabled ? (1 << ExtensionError::NUM_ERROR_TYPES) - 1 : 0;

  prefs_->UpdateExtensionPref(extension_id, kStoreExtensionErrorsPref,
                              std::make_unique<base::Value>(mask));
}

bool ErrorConsole::IsReportingEnabledForExtension(
    const std::string& extension_id) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!enabled_ || !crx_file::id_util::IdIsValid(extension_id))
    return false;

  return GetMaskForExtension(extension_id) != 0;
}

void ErrorConsole::UseDefaultReportingForExtension(
    const std::string& extension_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!enabled_ || !crx_file::id_util::IdIsValid(extension_id))
    return;

  prefs_->UpdateExtensionPref(extension_id, kStoreExtensionErrorsPref, nullptr);
}

void ErrorConsole::ReportError(std::unique_ptr<ExtensionError> error) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!enabled_ || !crx_file::id_util::IdIsValid(error->extension_id()))
    return;

  DCHECK_GE(error->level(), extension_misc::kMinimumSeverityToReportError)
      << "Errors less than severity warning should not be reported.";

  int mask = GetMaskForExtension(error->extension_id());
  if (!(mask & (1 << error->type())))
    return;

  const ExtensionError* weak_error = errors_.AddError(std::move(error));
  for (auto& observer : observers_)
    observer.OnErrorAdded(weak_error);
}

void ErrorConsole::RemoveErrors(const ErrorMap::Filter& filter) {
  std::set<std::string> affected_ids;
  errors_.RemoveErrors(filter, &affected_ids);
  for (auto& observer : observers_)
    observer.OnErrorsRemoved(affected_ids);
}

const ErrorList& ErrorConsole::GetErrorsForExtension(
    const std::string& extension_id) const {
  return errors_.GetErrorsForExtension(extension_id);
}

void ErrorConsole::AddObserver(Observer* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  observers_.AddObserver(observer);
}

void ErrorConsole::RemoveObserver(Observer* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  observers_.RemoveObserver(observer);
}

bool ErrorConsole::IsEnabledForChromeExtensionsPage() const {
  return profile_->GetPrefs()->GetBoolean(prefs::kExtensionsUIDeveloperMode);
}

bool ErrorConsole::IsEnabledForAppsDeveloperTools() const {
  return ExtensionRegistry::Get(profile_)->enabled_extensions()
      .Contains(kAppsDeveloperToolsExtensionId);
}

void ErrorConsole::CheckEnabled() {
  bool should_be_enabled = IsEnabledForChromeExtensionsPage() ||
                           IsEnabledForAppsDeveloperTools();
  if (should_be_enabled && !enabled_)
    Enable();
  if (!should_be_enabled && enabled_)
    Disable();
}

void ErrorConsole::Enable() {
  enabled_ = true;

  // We postpone the initialization of |prefs_| until now because they can be
  // nullptr in unit_tests. Any unit tests that enable the error console should
  // also create an ExtensionPrefs object.
  prefs_ = ExtensionPrefs::Get(profile_);

  profile_observer_.Add(profile_);
  if (profile_->HasOffTheRecordProfile())
    profile_observer_.Add(profile_->GetOffTheRecordProfile());

  const ExtensionSet& extensions =
      ExtensionRegistry::Get(profile_)->enabled_extensions();
  for (ExtensionSet::const_iterator iter = extensions.begin();
       iter != extensions.end();
       ++iter) {
    AddManifestErrorsForExtension(iter->get());
  }
}

void ErrorConsole::Disable() {
  profile_observer_.RemoveAll();
  errors_.RemoveAllErrors();
  enabled_ = false;
}

void ErrorConsole::OnPrefChanged() {
  CheckEnabled();
}

void ErrorConsole::OnExtensionUnloaded(content::BrowserContext* browser_context,
                                       const Extension* extension,
                                       UnloadedExtensionReason reason) {
  CheckEnabled();
}

void ErrorConsole::OnExtensionLoaded(content::BrowserContext* browser_context,
                                     const Extension* extension) {
  CheckEnabled();
}

void ErrorConsole::OnExtensionInstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    bool is_update) {
  // We don't want to have manifest errors from previous installs. We want
  // to keep runtime errors, though, because extensions are reloaded on a
  // refresh of chrome:extensions, and we don't want to wipe our history
  // whenever that happens.
  errors_.RemoveErrors(ErrorMap::Filter::ErrorsForExtensionWithType(
      extension->id(), ExtensionError::MANIFEST_ERROR), nullptr);
  AddManifestErrorsForExtension(extension);
}

void ErrorConsole::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    extensions::UninstallReason reason) {
  errors_.RemoveErrors(ErrorMap::Filter::ErrorsForExtension(extension->id()),
                       nullptr);
}

void ErrorConsole::AddManifestErrorsForExtension(const Extension* extension) {
  const std::vector<InstallWarning>& warnings =
      extension->install_warnings();
  for (auto iter = warnings.begin(); iter != warnings.end(); ++iter) {
    ReportError(std::unique_ptr<ExtensionError>(new ManifestError(
        extension->id(), base::UTF8ToUTF16(iter->message),
        base::UTF8ToUTF16(iter->key), base::UTF8ToUTF16(iter->specific))));
  }
}

void ErrorConsole::OnOffTheRecordProfileCreated(Profile* off_the_record) {
  profile_observer_.Add(off_the_record);
}

void ErrorConsole::OnProfileWillBeDestroyed(Profile* profile) {
  profile_observer_.Remove(profile);
  // If incognito profile which we are associated with is destroyed, also
  // destroy all incognito errors.
  if (profile->IsOffTheRecord() && profile_->IsSameProfile(profile))
    errors_.RemoveErrors(ErrorMap::Filter::IncognitoErrors(), nullptr);
}

int ErrorConsole::GetMaskForExtension(const std::string& extension_id) const {
  // Registered preferences take priority over everything else.
  int pref = 0;
  if (prefs_->ReadPrefAsInteger(extension_id, kStoreExtensionErrorsPref, &pref))
    return pref;

  // If the extension is unpacked, we report all error types by default.
  const Extension* extension =
      ExtensionRegistry::Get(profile_)->GetExtensionById(
          extension_id, ExtensionRegistry::EVERYTHING);
  if (extension && extension->location() == Manifest::UNPACKED)
    return (1 << ExtensionError::NUM_ERROR_TYPES) - 1;

  // Otherwise, use the default mask.
  return default_mask_;
}

}  // namespace extensions
