// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/tts_ash.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/speech/crosapi_tts_engine_delegate_ash.h"
#include "chrome/browser/speech/tts_crosapi_util.h"
#include "chromeos/crosapi/mojom/tts.mojom.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/tts_controller.h"

namespace crosapi {

TtsAsh::TtsAsh(ProfileManager* profile_manager)
    : profile_manager_(profile_manager),
      primary_profile_browser_context_id_(base::UnguessableToken::Null()) {
  DCHECK(profile_manager_);
  profile_manager_observation_.Observe(profile_manager);
  voices_changed_observation_.Observe(content::TtsController::GetInstance());
}

TtsAsh::~TtsAsh() = default;

void TtsAsh::BindReceiver(mojo::PendingReceiver<mojom::Tts> pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

bool TtsAsh::HasTtsClient() const {
  return tts_clients_.size() > 0;
}

base::UnguessableToken TtsAsh::GetPrimaryProfileBrowserContextId() const {
  return primary_profile_browser_context_id_;
}

void TtsAsh::RegisterTtsClient(mojo::PendingRemote<mojom::TtsClient> client,
                               const base::UnguessableToken& browser_context_id,
                               bool from_primary_profile) {
  DCHECK(from_primary_profile);
  if (from_primary_profile)
    primary_profile_browser_context_id_ = browser_context_id;

  // Note: This is a temporary workaround for enabling Lacros tts support in ash
  // when running Lacros tts extension api lacros browser tests.
  // TODO(crbug.com/1227543): Migrate to enable tts lacros support feature flag
  // in Ash before running lacros browser tests once the Lacros testing
  // infrasture adds that support.
  if (!tts_crosapi_util::ShouldEnableLacrosTtsSupport()) {
    // This code path is only called when running lacros browser tests.
    content::TtsController::GetInstance()->SetRemoteTtsEngineDelegate(
        CrosapiTtsEngineDelegateAsh::GetInstance());
  }

  mojo::Remote<mojom::TtsClient> remote(std::move(client));
  remote.set_disconnect_handler(base::BindOnce(&TtsAsh::TtsClientDisconnected,
                                               weak_ptr_factory_.GetWeakPtr(),
                                               browser_context_id));
  tts_clients_.emplace(browser_context_id, std::move(remote));
}

void TtsAsh::VoicesChanged(const base::UnguessableToken& browser_context_id,
                           std::vector<mojom::TtsVoicePtr> lacros_voices) {
  if (!HasTtsClient())
    return;

  // TODO(crbug.com/1251979): Support secondary profile.
  DCHECK(browser_context_id == primary_profile_browser_context_id_);

  std::vector<content::VoiceData> voices;
  for (const auto& mojo_voice : lacros_voices)
    voices.push_back(tts_crosapi_util::FromMojo(mojo_voice));

  // Cache Lacros voices.
  crosapi_voices_[browser_context_id] = std::move(voices);

  // Notify TtsController about VoicesChanged.
  content::TtsController::GetInstance()->VoicesChanged();
}

void TtsAsh::GetCrosapiVoices(base::UnguessableToken browser_context_id,
                              std::vector<content::VoiceData>* out_voices) {
  // Returns the cached Lacros voices.
  auto it_voices = crosapi_voices_.find(browser_context_id);
  if (it_voices != crosapi_voices_.end()) {
    for (auto voice : it_voices->second) {
      out_voices->push_back(voice);
    }
  }
}

void TtsAsh::OnVoicesChanged() {
  if (!HasTtsClient())
    return;

  // Notify Lacros about voices change in Ash's TtsController.
  // TtsController in ash manages all the voices from both Ash and Lacros,
  // which is the ultimate truth of source to return all the voices when
  // asked by Lacros.
  std::vector<content::VoiceData> all_voices;
  content::TtsController::GetInstance()->GetVoices(
      ProfileManager::GetActiveUserProfile(), GURL(), &all_voices);

  // Convert to mojo voices.
  std::vector<crosapi::mojom::TtsVoicePtr> mojo_voices;
  for (const auto& voice : all_voices)
    mojo_voices.push_back(tts_crosapi_util::ToMojo(voice));

  auto item = tts_clients_.find(primary_profile_browser_context_id_);
  DCHECK(item != tts_clients_.end());
  item->second->VoicesChanged(std::move(mojo_voices));
}

void TtsAsh::OnProfileAdded(Profile* profile) {
  if (tts_crosapi_util::ShouldEnableLacrosTtsSupport()) {
    content::TtsController::GetInstance()->SetRemoteTtsEngineDelegate(
        CrosapiTtsEngineDelegateAsh::GetInstance());
  }
}

void TtsAsh::OnProfileManagerDestroying() {
  profile_manager_observation_.Reset();
  profile_manager_ = nullptr;
}

void TtsAsh::TtsClientDisconnected(
    const base::UnguessableToken& browser_context_id) {
  tts_clients_.erase(browser_context_id);
  if (browser_context_id == primary_profile_browser_context_id_)
    primary_profile_browser_context_id_ = base::UnguessableToken::Null();

  // Remove the cached lacros voices.
  size_t erase_count = crosapi_voices_.erase(browser_context_id);
  if (erase_count > 0)
    content::TtsController::GetInstance()->VoicesChanged();
}

}  // namespace crosapi
