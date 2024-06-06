// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/embedded_a11y_manager_lacros.h"

#include <memory>
#include <optional>

#include "base/memory/singleton.h"
#include "base/path_service.h"
#include "chrome/browser/accessibility/embedded_a11y_extension_loader.h"
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
#include "ui/gfx/animation/animation.h"

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

  reduced_animations_enabled_observer_ = std::make_unique<CrosapiPrefObserver>(
      crosapi::mojom::PrefPath::kAccessibilityReducedAnimationsEnabled,
      base::BindRepeating(
          &EmbeddedA11yManagerLacros::OnReducedAnimationsEnabledChanged,
          weak_ptr_factory_.GetWeakPtr()));

  overscroll_history_navigation_enabled_observer_ =
      std::make_unique<CrosapiPrefObserver>(
          crosapi::mojom::PrefPath::kOverscrollHistoryNavigationEnabled,
          base::BindRepeating(&EmbeddedA11yManagerLacros::
                                  OnOverscrollHistoryNavigationEnabledChanged,
                              weak_ptr_factory_.GetWeakPtr()));

  EmbeddedA11yExtensionLoader::GetInstance()->Init();

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  profile_manager_observation_.Observe(profile_manager);

  // Observe all existing profiles.
  std::vector<Profile*> profiles =
      g_browser_process->profile_manager()->GetLoadedProfiles();
  for (auto* profile : profiles) {
    observed_profiles_.AddObservation(profile);
  }

  UpdateEmbeddedA11yHelperExtension();
  UpdateChromeVoxHelperExtension();
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

bool EmbeddedA11yManagerLacros::IsSelectToSpeakEnabled() {
  return select_to_speak_enabled_;
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
}

void EmbeddedA11yManagerLacros::OnProfileAdded(Profile* profile) {
  observed_profiles_.AddObservation(profile);
}

void EmbeddedA11yManagerLacros::OnProfileManagerDestroying() {
  profile_manager_observation_.Reset();
}

void EmbeddedA11yManagerLacros::UpdateOverscrollHistoryNavigationEnabled() {
  if (overscroll_history_navigation_enabled_.has_value()) {
    g_browser_process->local_state()->SetBoolean(
        prefs::kOverscrollHistoryNavigationEnabled,
        overscroll_history_navigation_enabled_.value());
  }
}

void EmbeddedA11yManagerLacros::OnChromeVoxEnabledChanged(base::Value value) {
  CHECK(value.is_bool());
  chromevox_enabled_ = value.GetBool();
  UpdateChromeVoxHelperExtension();
}

void EmbeddedA11yManagerLacros::OnSelectToSpeakEnabledChanged(
    base::Value value) {
  CHECK(value.is_bool());
  select_to_speak_enabled_ = value.GetBool();
  UpdateEmbeddedA11yHelperExtension();
}

void EmbeddedA11yManagerLacros::OnSwitchAccessEnabledChanged(
    base::Value value) {
  CHECK(value.is_bool());
  switch_access_enabled_ = value.GetBool();
  UpdateEmbeddedA11yHelperExtension();
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

void EmbeddedA11yManagerLacros::OnReducedAnimationsEnabledChanged(
    base::Value value) {
  CHECK(value.is_bool());
  gfx::Animation::SetPrefersReducedMotionForA11y(value.GetBool());
}

void EmbeddedA11yManagerLacros::OnOverscrollHistoryNavigationEnabledChanged(
    base::Value value) {
  CHECK(value.is_bool());
  overscroll_history_navigation_enabled_ = value.GetBool();
  UpdateOverscrollHistoryNavigationEnabled();
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

void EmbeddedA11yManagerLacros::SetReadingModeEnabled(bool enabled) {
  if (reading_mode_enabled_ != enabled) {
    reading_mode_enabled_ = enabled;
    UpdateEmbeddedA11yHelperExtension();
  }
}

bool EmbeddedA11yManagerLacros::IsReadingModeEnabled() {
  return reading_mode_enabled_;
}

void EmbeddedA11yManagerLacros::UpdateEmbeddedA11yHelperExtension() {
  // Switch Access and Select to Speak share a helper extension which has a
  // manifest content script to tell Google Docs to annotate the HTML canvas.
  if (select_to_speak_enabled_ || switch_access_enabled_ ||
      reading_mode_enabled_) {
    EmbeddedA11yExtensionLoader::GetInstance()->InstallExtensionWithId(
        extension_misc::kEmbeddedA11yHelperExtensionId,
        extension_misc::kEmbeddedA11yHelperExtensionPath,
        extension_misc::kEmbeddedA11yHelperManifestFilename,
        /*should_localize=*/true);
  } else {
    EmbeddedA11yExtensionLoader::GetInstance()->RemoveExtensionWithId(
        extension_misc::kEmbeddedA11yHelperExtensionId);
  }
}

void EmbeddedA11yManagerLacros::UpdateChromeVoxHelperExtension() {
  // ChromeVox has a helper extension which has a content script to tell Google
  // Docs that ChromeVox is enabled.
  if (chromevox_enabled_) {
    EmbeddedA11yExtensionLoader::GetInstance()->InstallExtensionWithId(
        extension_misc::kChromeVoxHelperExtensionId,
        extension_misc::kChromeVoxHelperExtensionPath,
        extension_misc::kChromeVoxHelperManifestFilename,
        /*should_localize=*/false);
  } else {
    EmbeddedA11yExtensionLoader::GetInstance()->RemoveExtensionWithId(
        extension_misc::kChromeVoxHelperExtensionId);
  }
}
