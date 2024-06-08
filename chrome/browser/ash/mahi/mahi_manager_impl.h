// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_MAHI_MAHI_MANAGER_IMPL_H_
#define CHROME_BROWSER_ASH_MAHI_MAHI_MANAGER_IMPL_H_

#include <memory>
#include <string>

#include "ash/public/cpp/session/session_observer.h"
#include "ash/session/session_controller_impl.h"
#include "ash/system/mahi/mahi_ui_controller.h"
#include "base/memory/raw_ptr.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ash/mahi/mahi_browser_delegate_ash.h"
#include "chrome/browser/ash/mahi/mahi_cache_manager.h"
#include "chromeos/components/mahi/public/cpp/mahi_manager.h"
#include "components/manta/mahi_provider.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

// Implementation of `MahiManager`.
class MahiManagerImpl : public chromeos::MahiManager, public SessionObserver {
 public:
  MahiManagerImpl();

  MahiManagerImpl(const MahiManagerImpl&) = delete;
  MahiManagerImpl& operator=(const MahiManagerImpl&) = delete;

  ~MahiManagerImpl() override;

  // chromeos::MahiManager:
  std::u16string GetContentTitle() override;
  gfx::ImageSkia GetContentIcon() override;
  GURL GetContentUrl() override;
  void GetSummary(MahiSummaryCallback callback) override;
  void GetOutlines(MahiOutlinesCallback callback) override;
  void GoToOutlineContent(int outline_id) override;
  void AnswerQuestion(const std::u16string& question,
                      bool current_panel_content,
                      MahiAnswerQuestionCallback callback) override;
  void GetSuggestedQuestion(MahiGetSuggestedQuestionCallback callback) override;
  void SetCurrentFocusedPageInfo(crosapi::mojom::MahiPageInfoPtr info) override;
  void OnContextMenuClicked(
      crosapi::mojom::MahiContextMenuRequestPtr context_menu_request) override;
  void OpenFeedbackDialog() override;
  bool IsEnabled() override;
  void SetMediaAppPDFFocused() override;
  void MediaAppPDFClosed(
      const base::UnguessableToken media_app_client_id) override;
  std::optional<base::UnguessableToken> GetMediaAppPDFClientId() const override;

  // Notifies the panel that refresh is available or not for the corresponding
  // surface.
  void NotifyRefreshAvailability(bool available);

  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;

 private:
  friend class MahiManagerImplTest;
  friend class MahiManagerImplFeatureKeyTest;

  void OnMahiPrefChanged();

  // Initialize required provider if it is not initialized yet, and discard
  // pending requests to avoid racing condition.
  // Returns true if successfully initialized.
  bool MaybeInitializeAndDiscardPendingRequests();

  void OnGetPageContentForSummary(
      crosapi::mojom::MahiPageInfoPtr request_page_info,
      MahiSummaryCallback callback,
      crosapi::mojom::MahiPageContentPtr mahi_content_ptr);

  void OnGetPageContentForQA(
      crosapi::mojom::MahiPageInfoPtr request_page_info,
      const std::u16string& question,
      MahiAnswerQuestionCallback callback,
      crosapi::mojom::MahiPageContentPtr mahi_content_ptr);

  void OnMahiProviderSummaryResponse(
      crosapi::mojom::MahiPageInfoPtr request_page_info,
      MahiSummaryCallback summary_callback,
      base::Value::Dict dict,
      manta::MantaStatus status);

  void OnMahiProviderQAResponse(
      crosapi::mojom::MahiPageInfoPtr request_page_info,
      const std::u16string& question,
      MahiAnswerQuestionCallback callback,
      base::Value::Dict dict,
      manta::MantaStatus status);

  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
  base::ScopedObservation<SessionController, SessionObserver>
      session_observation_{this};

  // These `Ptr`s should never be null. To invalidate them, assign them a
  // `New()` instead of calling `reset()`.
  crosapi::mojom::MahiPageInfoPtr current_page_info_ =
      crosapi::mojom::MahiPageInfo::New();

  crosapi::mojom::MahiPageContentPtr current_panel_content_ =
      crosapi::mojom::MahiPageContent::New();

  // Stores metadata of the current content in the panel.
  crosapi::mojom::MahiPageInfoPtr current_panel_info_ =
      crosapi::mojom::MahiPageInfo::New();

  // Pair of question and their corresponding answer for the current panel
  // content
  std::vector<std::pair<std::string, std::string>> current_panel_qa_;

  std::unique_ptr<manta::MahiProvider> mahi_provider_;

  raw_ptr<ash::MahiBrowserDelegateAsh> mahi_browser_delegate_ash_ = nullptr;

  // Keeps track of the latest result and code, used for feedback.
  std::u16string latest_summary_;
  chromeos::MahiResponseStatus latest_response_status_;

  MahiUiController ui_controller_;

  std::unique_ptr<MahiCacheManager> cache_manager_;

  // If true, tries to get content from MediaAppContentManager instead.
  bool media_app_pdf_focused_ = false;
  base::UnguessableToken media_app_client_id_;

  base::WeakPtrFactory<MahiManagerImpl> weak_ptr_factory_for_requests_{this};
  base::WeakPtrFactory<MahiManagerImpl> weak_ptr_factory_for_pref_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_MAHI_MAHI_MANAGER_IMPL_H_
