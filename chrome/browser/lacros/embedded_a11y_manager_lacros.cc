// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/embedded_a11y_manager_lacros.h"

#include <memory>

#include "base/memory/singleton.h"
#include "base/path_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/api/accessibility_service_private.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chromeos/crosapi/mojom/embedded_accessibility_helper.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension_l10n_util.h"
#include "extensions/common/file_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

absl::optional<base::Value::Dict> LoadManifestOnFileThread(
    const base::FilePath& path,
    const base::FilePath::CharType* manifest_filename,
    bool localize) {
  CHECK(extensions::GetExtensionFileTaskRunner()->RunsTasksInCurrentSequence());
  std::string error;
  auto manifest =
      extensions::file_util::LoadManifest(path, manifest_filename, &error);
  if (!manifest) {
    LOG(ERROR) << "Can't load " << path.Append(manifest_filename).AsUTF8Unsafe()
               << ": " << error;
    return absl::nullopt;
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

// static
EmbeddedA11yManagerLacros* EmbeddedA11yManagerLacros::GetInstance() {
  return base::Singleton<
      EmbeddedA11yManagerLacros,
      base::LeakySingletonTraits<EmbeddedA11yManagerLacros>>::get();
}

EmbeddedA11yManagerLacros::EmbeddedA11yManagerLacros() = default;

EmbeddedA11yManagerLacros::~EmbeddedA11yManagerLacros() = default;

void EmbeddedA11yManagerLacros::ClipboardCopyInActiveGoogleDoc(
    const std::string& url) {
  // Get the `Profile` last used (the `Profile` which owns the most
  // recently focused window). This is the one on which we want to
  // request speech.
  Profile* profile = ProfileManager::GetLastUsedProfile();
  extensions::EventRouter* event_router = extensions::EventRouter::Get(profile);

  auto event_args(extensions::api::accessibility_service_private::
                      ClipboardCopyInActiveGoogleDoc::Create(url));
  std::unique_ptr<extensions::Event> event(new extensions::Event(
      extensions::events::
          ACCESSIBILITY_SERVICE_PRIVATE_CLIPBOARD_COPY_IN_ACTIVE_GOOGLE_DOC,
      extensions::api::accessibility_service_private::
          ClipboardCopyInActiveGoogleDoc::kEventName,
      std::move(event_args)));
  event_router->DispatchEventWithLazyListener(
      extension_misc::kEmbeddedA11yHelperExtensionId, std::move(event));
}

void EmbeddedA11yManagerLacros::Init() {
  CHECK(!chromevox_enabled_observer_)
      << "EmbeddedA11yManagerLacros::Init should only be called once.";
  // Initial values are obtained when the observers are created, there is no
  // need to do so explicitly.
  chromevox_enabled_observer_ = std::make_unique<CrosapiPrefObserver>(
      crosapi::mojom::PrefPath::kAccessibilitySpokenFeedbackEnabled,
      base::BindRepeating(&EmbeddedA11yManagerLacros::OnChromeVoxEnabledChanged,
                          weak_ptr_factory_.GetWeakPtr()));
  select_to_speak_enabled_observer_ = std::make_unique<CrosapiPrefObserver>(
      crosapi::mojom::PrefPath::kAccessibilitySelectToSpeakEnabled,
      base::BindRepeating(
          &EmbeddedA11yManagerLacros::OnSelectToSpeakEnabledChanged,
          weak_ptr_factory_.GetWeakPtr()));
  switch_access_enabled_observer_ = std::make_unique<CrosapiPrefObserver>(
      crosapi::mojom::PrefPath::kAccessibilitySwitchAccessEnabled,
      base::BindRepeating(
          &EmbeddedA11yManagerLacros::OnSwitchAccessEnabledChanged,
          weak_ptr_factory_.GetWeakPtr()));

  chromeos::LacrosService* impl = chromeos::LacrosService::Get();
  if (impl->IsAvailable<
          crosapi::mojom::EmbeddedAccessibilityHelperClientFactory>()) {
    auto& remote = impl->GetRemote<
        crosapi::mojom::EmbeddedAccessibilityHelperClientFactory>();
    remote->BindEmbeddedAccessibilityHelperClient(
        a11y_helper_remote_.BindNewPipeAndPassReceiver());
    remote->BindEmbeddedAccessibilityHelper(
        a11y_helper_receiver_.BindNewPipeAndPassRemote());
  }

  if (impl->GetInterfaceVersion<
          crosapi::mojom::EmbeddedAccessibilityHelperClient>() >=
      static_cast<int>(crosapi::mojom::EmbeddedAccessibilityHelperClient::
                           kFocusChangedMinVersion)) {
    // Only observe focus highlight pref if the Ash version is able to support
    // focus highlight enabled changed. Otherwise this just adds overhead.
    focus_highlight_enabled_observer_ = std::make_unique<CrosapiPrefObserver>(
        crosapi::mojom::PrefPath::kAccessibilityFocusHighlightEnabled,
        base::BindRepeating(
            &EmbeddedA11yManagerLacros::OnFocusHighlightEnabledChanged,
            weak_ptr_factory_.GetWeakPtr()));
  }

  pdf_ocr_always_active_observer_ = std::make_unique<CrosapiPrefObserver>(
      crosapi::mojom::PrefPath::kAccessibilityPdfOcrAlwaysActive,
      base::BindRepeating(
          &EmbeddedA11yManagerLacros::OnPdfOcrAlwaysActiveChanged,
          weak_ptr_factory_.GetWeakPtr()));

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  profile_manager_observation_.Observe(profile_manager);

  // Observe all existing profiles.
  std::vector<Profile*> profiles =
      g_browser_process->profile_manager()->GetLoadedProfiles();
  for (auto* profile : profiles) {
    observed_profiles_.AddObservation(profile);
  }

  UpdateAllProfiles();
}

void EmbeddedA11yManagerLacros::SpeakSelectedText() {
  // Check the remote is bound. It might not be bound on older versions
  // of Ash.
  if (a11y_helper_remote_.is_bound()) {
    a11y_helper_remote_->SpeakSelectedText();
  }
  if (speak_selected_text_callback_for_test_) {
    speak_selected_text_callback_for_test_.Run();
  }
}

void EmbeddedA11yManagerLacros::AddExtensionChangedCallbackForTest(
    base::RepeatingClosure callback) {
  extension_installation_changed_callback_for_test_ = std::move(callback);
}

void EmbeddedA11yManagerLacros::AddSpeakSelectedTextCallbackForTest(
    base::RepeatingClosure callback) {
  speak_selected_text_callback_for_test_ = std::move(callback);
}

void EmbeddedA11yManagerLacros::AddFocusChangedCallbackForTest(
    base::RepeatingCallback<void(gfx::Rect)> callback) {
  focus_changed_callback_for_test_ = std::move(callback);
}

void EmbeddedA11yManagerLacros::OnProfileWillBeDestroyed(Profile* profile) {
  observed_profiles_.RemoveObservation(profile);
}

void EmbeddedA11yManagerLacros::OnOffTheRecordProfileCreated(
    Profile* off_the_record) {
  observed_profiles_.AddObservation(off_the_record);
  UpdateProfile(off_the_record);
}

void EmbeddedA11yManagerLacros::OnProfileAdded(Profile* profile) {
  observed_profiles_.AddObservation(profile);
  UpdateProfile(profile);
}

void EmbeddedA11yManagerLacros::OnProfileManagerDestroying() {
  profile_manager_observation_.Reset();
}

void EmbeddedA11yManagerLacros::UpdateAllProfiles() {
  std::vector<Profile*> profiles =
      g_browser_process->profile_manager()->GetLoadedProfiles();
  for (auto* profile : profiles) {
    UpdateProfile(profile);
    if (profile->HasAnyOffTheRecordProfile()) {
      const auto& otr_profiles = profile->GetAllOffTheRecordProfiles();
      for (auto* otr_profile : otr_profiles) {
        UpdateProfile(otr_profile);
      }
    }
  }
}

void EmbeddedA11yManagerLacros::UpdateProfile(Profile* profile) {
  // Switch Access and Select to Speak share a helper extension which has a
  // manifest content script to tell Google Docs to annotate the HTML canvas.
  if (select_to_speak_enabled_ || switch_access_enabled_) {
    MaybeInstallExtension(profile,
                          extension_misc::kEmbeddedA11yHelperExtensionId,
                          extension_misc::kEmbeddedA11yHelperExtensionPath,
                          extension_misc::kEmbeddedA11yHelperManifestFilename);
  } else {
    MaybeRemoveExtension(profile,
                         extension_misc::kEmbeddedA11yHelperExtensionId);
  }
  // ChromeVox has a helper extension which has a content script to tell Google
  // Docs that ChromeVox is enabled.
  if (chromevox_enabled_) {
    MaybeInstallExtension(profile, extension_misc::kChromeVoxHelperExtensionId,
                          extension_misc::kChromeVoxHelperExtensionPath,
                          extension_misc::kChromeVoxHelperManifestFilename);
  } else {
    MaybeRemoveExtension(profile, extension_misc::kChromeVoxHelperExtensionId);
  }

  PrefService* const pref_service = profile->GetPrefs();
  CHECK(pref_service);
  pref_service->SetBoolean(::prefs::kAccessibilityPdfOcrAlwaysActive,
                           pdf_ocr_always_active_enabled_);
}

void EmbeddedA11yManagerLacros::OnChromeVoxEnabledChanged(base::Value value) {
  CHECK(value.is_bool());
  chromevox_enabled_ = value.GetBool();
  UpdateAllProfiles();
}

void EmbeddedA11yManagerLacros::OnSelectToSpeakEnabledChanged(
    base::Value value) {
  CHECK(value.is_bool());
  select_to_speak_enabled_ = value.GetBool();
  UpdateAllProfiles();
}

void EmbeddedA11yManagerLacros::OnSwitchAccessEnabledChanged(
    base::Value value) {
  CHECK(value.is_bool());
  switch_access_enabled_ = value.GetBool();
  UpdateAllProfiles();
}

void EmbeddedA11yManagerLacros::OnFocusHighlightEnabledChanged(
    base::Value value) {
  CHECK(value.is_bool());
  if (value.GetBool()) {
    focus_changed_subscription_ =
        content::BrowserAccessibilityState::GetInstance()
            ->RegisterFocusChangedCallback(base::BindRepeating(
                &EmbeddedA11yManagerLacros::OnFocusChangedInPage,
                weak_ptr_factory_.GetWeakPtr()));
  } else {
    focus_changed_subscription_ = {};
  }
}

void EmbeddedA11yManagerLacros::OnPdfOcrAlwaysActiveChanged(base::Value value) {
  // TODO(b/289009784): Add browser test to ensure the pref is synced on all
  // profiles.
  CHECK(value.is_bool());
  pdf_ocr_always_active_enabled_ = value.GetBool();
  UpdateAllProfiles();
}

void EmbeddedA11yManagerLacros::MaybeRemoveExtension(
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

void EmbeddedA11yManagerLacros::MaybeInstallExtension(
    Profile* profile,
    const std::string& extension_id,
    const std::string& extension_path,
    const base::FilePath::CharType* manifest_name) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto* component_loader = GetComponentLoader(profile);
  if (!component_loader || component_loader->Exists(extension_id)) {
    return;
  }

  base::FilePath resources_path;
  if (!base::PathService::Get(chrome::DIR_RESOURCES, &resources_path)) {
    NOTREACHED();
  }
  auto path = resources_path.Append(extension_path);

  extensions::GetExtensionFileTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&LoadManifestOnFileThread, path, manifest_name,
                     /*localize=*/extension_id ==
                         extension_misc::kEmbeddedA11yHelperExtensionId),
      base::BindOnce(&EmbeddedA11yManagerLacros::InstallExtension,
                     weak_ptr_factory_.GetWeakPtr(), component_loader, path,
                     extension_id));
}

void EmbeddedA11yManagerLacros::InstallExtension(
    extensions::ComponentLoader* component_loader,
    const base::FilePath& path,
    const std::string& extension_id,
    absl::optional<base::Value::Dict> manifest) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (component_loader->Exists(extension_id)) {
    // Because this is async and called from another thread, it's possible we
    // already installed the extension. Don't try and reinstall in that case.
    // This may happen on init, for example, when ash a11y feature state and
    // new profiles are loaded all at the same time.
    return;
  }

  CHECK(manifest) << "Unable to load extension manifest for extension "
                  << extension_id;
  std::string actual_id =
      component_loader->Add(std::move(manifest.value()), path);
  CHECK_EQ(actual_id, extension_id);
  if (extension_installation_changed_callback_for_test_) {
    extension_installation_changed_callback_for_test_.Run();
  }
}

void EmbeddedA11yManagerLacros::OnFocusChangedInPage(
    const content::FocusedNodeDetails& details) {
  if (a11y_helper_remote_.is_bound()) {
    a11y_helper_remote_->FocusChanged(details.node_bounds_in_screen);
  }
  if (focus_changed_callback_for_test_) {
    focus_changed_callback_for_test_.Run(details.node_bounds_in_screen);
  }
}
