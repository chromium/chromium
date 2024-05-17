// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SPARKY_SPARKY_MANAGER_IMPL_H_
#define CHROME_BROWSER_ASH_SPARKY_SPARKY_MANAGER_IMPL_H_

#include "ash/system/mahi/mahi_ui_controller.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/components/mahi/public/cpp/mahi_manager.h"
#include "components/manta/mahi_provider.h"
#include "components/manta/manta_service.h"
#include "components/manta/sparky/sparky_provider.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

// The Mahi UI will be temporarily re-used for this feature which is an
// experimental prototype. For this reason, the SparkyManagerImpl derives from
// MahiManger, which contains the controls for the Mahi Ui.
class SparkyManagerImpl : public chromeos::MahiManager, public KeyedService {
 public:
  SparkyManagerImpl(Profile* profile, manta::MantaService* manta_service);

  SparkyManagerImpl(const SparkyManagerImpl&) = delete;
  SparkyManagerImpl& operator=(const SparkyManagerImpl&) = delete;

  ~SparkyManagerImpl() override;

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

  // Notifies the panel that refresh is available or not for the corresponding
  // surface.
  void NotifyRefreshAvailability(bool available);

 private:
  friend class SparkyManagerImplTest;
  friend class SparkyManagerImplFeatureKeyTest;

  // Initialize required provider if it is not initialized yet.
  void MaybeInitialize();

  void OnGetPageContentForSummary(
      MahiSummaryCallback callback,
      crosapi::mojom::MahiPageContentPtr mahi_content_ptr);

  void OnGetPageContentForQA(
      const std::u16string& question,
      MahiAnswerQuestionCallback callback,
      crosapi::mojom::MahiPageContentPtr mahi_content_ptr);

  void OnSparkyProviderQAResponse(const std::u16string& question,
                                  MahiAnswerQuestionCallback callback,
                                  const std::string& response,
                                  manta::MantaStatus status);

  crosapi::mojom::MahiPageInfoPtr current_page_info_ =
      crosapi::mojom::MahiPageInfo::New();

  crosapi::mojom::MahiPageContentPtr current_panel_content_ =
      crosapi::mojom::MahiPageContent::New();

  raw_ptr<Profile> profile_;

  // Pair of question and their corresponding answer for the current panel
  // content.
  std::vector<std::pair<std::string, std::string>> current_panel_qa_;

  std::unique_ptr<manta::SparkyProvider> sparky_provider_;

  // Keeps track of the latest result and code, used for feedback.
  std::u16string latest_summary_;
  chromeos::MahiResponseStatus latest_response_status_;

  MahiUiController ui_controller_;

  base::WeakPtrFactory<SparkyManagerImpl> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SPARKY_SPARKY_MANAGER_IMPL_H_
