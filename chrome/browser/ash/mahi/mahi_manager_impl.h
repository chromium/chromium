// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_MAHI_MAHI_MANAGER_IMPL_H_
#define CHROME_BROWSER_ASH_MAHI_MAHI_MANAGER_IMPL_H_

#include <memory>
#include <string>

#include "ash/public/cpp/session/session_observer.h"
#include "ash/session/session_controller_impl.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/components/mahi/public/cpp/mahi_manager.h"
#include "components/manta/mahi_provider.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace ash {

// Implementation of `MahiManager`.
class MahiManagerImpl : public chromeos::MahiManager, public SessionObserver {
 public:
  MahiManagerImpl();

  MahiManagerImpl(const MahiManagerImpl&) = delete;
  MahiManagerImpl& operator=(const MahiManagerImpl&) = delete;

  ~MahiManagerImpl() override;

  // chromeos::MahiManager:
  void OpenMahiPanel(int64_t display_id) override;
  std::u16string GetContentTitle() override;
  gfx::ImageSkia GetContentIcon() override;
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

  // Notifies the panel that refresh is available or not for the corresponding
  // surface.
  void NotifyRefreshAvailability(bool available);

  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;

 private:
  friend class MahiManagerImplTest;
  friend class MahiManagerImplFeatureKeyTest;

  void OnMahiPrefChanged();

  // Initialize required provider if it is not initialized yet.
  void MaybeInitialize();

  void OnGetPageContentForSummary(
      MahiSummaryCallback callback,
      crosapi::mojom::MahiPageContentPtr mahi_content_ptr);

  void OnGetPageContentForQA(
      const std::u16string& question,
      MahiAnswerQuestionCallback callback,
      crosapi::mojom::MahiPageContentPtr mahi_content_ptr);

  void OnMahiProviderSummaryResponse(MahiSummaryCallback summary_callback,
                                     base::Value::Dict dict,
                                     manta::MantaStatus status);

  void OnMahiProviderQAResponse(const std::u16string& question,
                                MahiAnswerQuestionCallback callback,
                                base::Value::Dict dict,
                                manta::MantaStatus status);

  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
  base::ScopedObservation<SessionController, SessionObserver>
      session_observation_{this};

  crosapi::mojom::MahiPageInfoPtr current_page_info_ =
      crosapi::mojom::MahiPageInfo::New();

  crosapi::mojom::MahiPageContentPtr current_panel_content_ =
      crosapi::mojom::MahiPageContent::New();

  GURL current_panel_url_;

  // Pair of question and their corresponding answer for the current panel
  // content
  std::vector<std::pair<std::string, std::string>> current_panel_qa_;

  std::unique_ptr<manta::MahiProvider> mahi_provider_;

  // Keeps track of the latest result and code, used for feedback.
  std::u16string latest_summary_;
  chromeos::MahiResponseStatus latest_response_status_;

  // The widget contains the Mahi main panel.
  views::UniqueWidgetPtr mahi_panel_widget_;

  base::WeakPtrFactory<MahiManagerImpl> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_MAHI_MAHI_MANAGER_IMPL_H_
