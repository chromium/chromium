// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/embedded_a11y_extension_loader.h"

#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension_l10n_util.h"
#include "extensions/common/file_util.h"

namespace {

std::optional<base::Value::Dict> LoadManifestOnFileThread(
    const base::FilePath& path,
    const base::FilePath::CharType* manifest_filename,
    bool localize) {
  CHECK(extensions::GetExtensionFileTaskRunner()->RunsTasksInCurrentSequence());
  std::string error;
  auto manifest =
      extensions::file_util::LoadManifest(path, manifest_filename, &error);
  if (!manifest) {
    std::ostringstream errorStream;
    errorStream << "Can't load "
                << path.Append(manifest_filename).AsUTF8Unsafe() << ": "
                << error;
    LOG(ERROR) << errorStream.str();
    static auto* const crash_key = base::debug::AllocateCrashKeyString(
        "helper_extension_failure", base::debug::CrashKeySize::Size1024);
    base::debug::SetCrashKeyString(crash_key, errorStream.str());
    base::debug::DumpWithoutCrashing();
    return std::nullopt;
  }
  if (localize) {
    // This is only called for Lacros component extensions which are loaded
    // from a read-only rootfs partition, so it is safe to set
    // |gzip_permission| to kAllowForTrustedSource.
    bool localized = extension_l10n_util::LocalizeExtension(
        path, &manifest.value(),
        extension_l10n_util::GzippedMessagesPermission::kAllowForTrustedSource,
        &error);
    CHECK(localized) << error;
  }
  return manifest;
}

extensions::ComponentLoader* GetComponentLoader(Profile* profile) {
  auto* extension_system = extensions::ExtensionSystem::Get(profile);
  if (!extension_system) {
    // May be missing on the Lacros login profile.
    return nullptr;
  }
  auto* extension_service = extension_system->extension_service();
  if (!extension_service) {
    return nullptr;
  }
  return extension_service->component_loader();
}

}  // namespace

EmbeddedA11yExtensionLoader::ExtensionInfo::ExtensionInfo(
    const std::string& extension_id,
    const std::string& extension_path,
    const base::FilePath::CharType* extension_manifest_file,
    bool should_localize)
    : extension_id(extension_id),
      extension_path(extension_path),
      extension_manifest_file(extension_manifest_file),
      should_localize(should_localize) {}
EmbeddedA11yExtensionLoader::ExtensionInfo::ExtensionInfo(
    const ExtensionInfo& other) = default;
EmbeddedA11yExtensionLoader::ExtensionInfo::ExtensionInfo(ExtensionInfo&&) =
    default;
EmbeddedA11yExtensionLoader::ExtensionInfo::~ExtensionInfo() = default;

// static
EmbeddedA11yExtensionLoader* EmbeddedA11yExtensionLoader::GetInstance() {
  return base::Singleton<
      EmbeddedA11yExtensionLoader,
      base::LeakySingletonTraits<EmbeddedA11yExtensionLoader>>::get();
}

EmbeddedA11yExtensionLoader::EmbeddedA11yExtensionLoader() = default;

EmbeddedA11yExtensionLoader::~EmbeddedA11yExtensionLoader() = default;

void EmbeddedA11yExtensionLoader::Init() {
  if (initialized_) {
    return;
  }

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  profile_manager_observation_.Observe(profile_manager);

  // Observe all existing profiles.
  std::vector<Profile*> profiles =
      g_browser_process->profile_manager()->GetLoadedProfiles();
  for (auto* profile : profiles) {
    observed_profiles_.AddObservation(profile);
  }
  for (const auto& extension : extension_map_) {
    UpdateAllProfiles(extension.first);
  }
  initialized_ = true;
}

void EmbeddedA11yExtensionLoader::InstallExtensionWithId(
    const std::string& extension_id,
    const std::string& extension_path,
    const base::FilePath::CharType* manifest_name,
    bool should_localize) {
  if (extension_map_.contains(extension_id)) {
    return;
  }

  ExtensionInfo new_extension = {extension_id, extension_path, manifest_name,
                                 should_localize};
  extension_map_.insert({extension_id, new_extension});
  UpdateAllProfiles(extension_id);
}

void EmbeddedA11yExtensionLoader::RemoveExtensionWithId(
    const std::string& extension_id) {
  if (!extension_map_.contains(extension_id)) {
    return;
  }

  extension_map_.erase(extension_id);
  UpdateAllProfiles(extension_id);
}

void EmbeddedA11yExtensionLoader::AddExtensionChangedCallbackForTest(
    base::RepeatingClosure callback) {
  extension_installation_changed_callback_for_test_ = std::move(callback);
}

void EmbeddedA11yExtensionLoader::OnProfileWillBeDestroyed(Profile* profile) {
  observed_profiles_.RemoveObservation(profile);
}

void EmbeddedA11yExtensionLoader::OnOffTheRecordProfileCreated(
    Profile* off_the_record) {
  observed_profiles_.AddObservation(off_the_record);
  for (const auto& extension : extension_map_) {
    UpdateProfile(off_the_record, extension.first);
  }
}

void EmbeddedA11yExtensionLoader::OnProfileAdded(Profile* profile) {
  observed_profiles_.AddObservation(profile);
  for (const auto& extension : extension_map_) {
    UpdateProfile(profile, extension.first);
  }
}

void EmbeddedA11yExtensionLoader::OnProfileManagerDestroying() {
  profile_manager_observation_.Reset();
}

void EmbeddedA11yExtensionLoader::UpdateAllProfiles(
    const std::string& extension_id) {
  std::vector<Profile*> profiles =
      g_browser_process->profile_manager()->GetLoadedProfiles();
  for (auto* profile : profiles) {
    UpdateProfile(profile, extension_id);
    if (profile->HasAnyOffTheRecordProfile()) {
      const auto& otr_profiles = profile->GetAllOffTheRecordProfiles();
      for (auto* otr_profile : otr_profiles) {
        UpdateProfile(otr_profile, extension_id);
      }
    }
  }
}

void EmbeddedA11yExtensionLoader::UpdateProfile(
    Profile* profile,
    const std::string& extension_id) {
  if (extension_map_.contains(extension_id)) {
    ExtensionInfo& extension = extension_map_.at(extension_id);
    MaybeInstallExtension(profile, extension_id, extension.extension_path,
                          extension.extension_manifest_file,
                          extension.should_localize);
  } else {
    MaybeRemoveExtension(profile, extension_id);
  }
}

void EmbeddedA11yExtensionLoader::MaybeRemoveExtension(
    Profile* profile,
    const std::string& extension_id) {
  auto* component_loader = GetComponentLoader(profile);
  if (!component_loader || !component_loader->Exists(extension_id)) {
    return;
  }
  component_loader->Remove(extension_id);
  if (extension_installation_changed_callback_for_test_) {
    extension_installation_changed_callback_for_test_.Run();
  }
}

void EmbeddedA11yExtensionLoader::MaybeInstallExtension(
    Profile* profile,
    const std::string& extension_id,
    const std::string& extension_path,
    const base::FilePath::CharType* manifest_name,
    bool should_localize) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto* component_loader = GetComponentLoader(profile);
  if (!component_loader || component_loader->Exists(extension_id)) {
    return;
  }

  base::FilePath resources_path;
#if BUILDFLAG(IS_MAC)
  base::FilePath root_path;
  CHECK(base::PathService::Get(base::DIR_MODULE, &root_path));
  resources_path = root_path.Append("resources");
#else
  if (!base::PathService::Get(chrome::DIR_RESOURCES, &resources_path)) {
    NOTREACHED_IN_MIGRATION();
  }
#endif

  base::FilePath::StringType common_path;
#if BUILDFLAG(IS_WIN)
  common_path = base::UTF8ToWide(extension_path);
#else
  common_path = extension_path;
#endif

  auto path = resources_path.Append(common_path);

  extensions::GetExtensionFileTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&LoadManifestOnFileThread, path, manifest_name,
                     /*localize=*/should_localize),
      base::BindOnce(&EmbeddedA11yExtensionLoader::InstallExtension,
                     weak_ptr_factory_.GetWeakPtr(), component_loader, path,
                     extension_id));
}

void EmbeddedA11yExtensionLoader::InstallExtension(
    extensions::ComponentLoader* component_loader,
    const base::FilePath& path,
    const std::string& extension_id,
    std::optional<base::Value::Dict> manifest) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (component_loader->Exists(extension_id)) {
    // Because this is async and called from another thread, it's possible we
    // already installed the extension. Don't try and reinstall in that case.
    // This may happen on init, for example, when ash a11y feature state and
    // new profiles are loaded all at the same time.
    return;
  }

// TODO(b/324143642): Extension manifest file should not be null.
// Temporarily logging the error to prevent crashes while we diagnose why it's
// null.
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
  if (!manifest) {
    LOG(ERROR) << "Unable to load extension manifest for extension "
               << extension_id << "; Path: " << path;
    return;
  }
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)
  CHECK(manifest) << "Unable to load extension manifest for extension "
                  << extension_id << "; Path: " << path;
  std::string actual_id =
      component_loader->Add(std::move(manifest.value()), path);
  CHECK_EQ(actual_id, extension_id);
  if (extension_installation_changed_callback_for_test_) {
    extension_installation_changed_callback_for_test_.Run();
  }
}

bool EmbeddedA11yExtensionLoader::IsExtensionInstalled(
    const std::string& extension_id) {
  return extension_map_.contains(extension_id);
}
