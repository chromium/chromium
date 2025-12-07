// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_CORAL_CORAL_CONTROLLER_H_
#define ASH_WM_CORAL_CORAL_CONTROLLER_H_

#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ash/birch/coral_constants.h"
#include "ash/public/cpp/scanner/scanner_feedback_info.h"
#include "base/memory/weak_ptr.h"
#include "base/token.h"
#include "chromeos/ash/services/coral/public/mojom/coral_service.mojom.h"
#include "components/desks_storage/core/desk_model.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace aura {
class Window;
class WindowTracker;
}  // namespace aura

namespace ash {

class Desk;
class DeskTemplate;
class FakeCoralProcessor;

class ASH_EXPORT CoralRequest {
 public:
  using ContentItem = coral::mojom::EntityPtr;

  CoralRequest();
  CoralRequest(const CoralRequest&) = delete;
  CoralRequest& operator=(const CoralRequest&) = delete;
  ~CoralRequest();

  void set_source(CoralSource source) { source_ = source; }
  CoralSource source() const { return source_; }

  void set_content(std::vector<ContentItem> content) {
    content_ = std::move(content);
  }
  const std::vector<ContentItem>& content() const { return content_; }

  void set_suppression_context(std::vector<ContentItem>&& suppression_context) {
    suppression_context_ = std::move(suppression_context);
  }
  const std::vector<ContentItem>& suppression_context() const {
    return suppression_context_;
  }

  void set_language(const std::string& language) { language_ = language; }

  std::string language() const { return language_; }

  std::string ToString() const;

 private:
  CoralSource source_ = CoralSource::kUnknown;

  // Tab/app content with arbitrary ordering.
  std::vector<ContentItem> content_;

  // Original tab/app content of the workspace.
  std::vector<ContentItem> suppression_context_;

  // The language code, e.g., "en" for English.
  std::string language_;
};

// `CoralResponse` contains 0-2 groups in order of relevance.
class ASH_EXPORT CoralResponse {
 public:
  using Group = coral::mojom::GroupPtr;

  CoralResponse();
  CoralResponse(const CoralResponse&) = delete;
  CoralResponse& operator=(const CoralResponse&) = delete;
  ~CoralResponse();

  void set_source(CoralSource source) { source_ = source; }

  void set_groups(std::vector<Group>&& groups) { groups_ = std::move(groups); }

  CoralSource source() const { return source_; }

  const std::vector<Group>& groups() const { return groups_; }
  std::vector<Group>& groups() { return groups_; }

 private:
  CoralSource source_ = CoralSource::kUnknown;
  std::vector<Group> groups_;
};

// Controller interface of the coral feature.
class ASH_EXPORT CoralController {
 public:
  CoralController();
  CoralController(const CoralController&) = delete;
  CoralController& operator=(const CoralController&) = delete;
  ~CoralController();

  // Claims necessary resources (dlc download / model loading) for processing
  // `GenerateContentGroups` and `CacheEmbeddings` requests. This should be
  // run before other methods to ensure models are ready, and set up the
  // language.
  void Initialize(std::string language);

  // GenerateContentGroups clusters the input ContentItems (which includes web
  // tabs, apps, etc.) into suitable groups based on their topics, and gives
  // each group a suitable title. If GenerateContentGroups request failed,
  // nullptr will be returned.
  // If `title_observer` is non-null, the backend will function in an async
  // title generation mode, where `callback` will be triggered as soon as the
  // grouping is done, but with empty titles. Then, `title_observer` will be
  // triggered once for each group when their title is generated. If it's null,
  // the backend will return the titles together with the response.
  // This design is because title generation may take significantly longer
  // compared to rest of the grouping process, so receiving the response before
  // title is updated will allow UI to show the groupings with a loading title,
  // enhancing the user experience.
  using CoralResponseCallback =
      base::OnceCallback<void(std::unique_ptr<CoralResponse>)>;
  void GenerateContentGroups(
      const CoralRequest& request,
      mojo::PendingRemote<coral::mojom::TitleObserver> title_observer,
      CoralResponseCallback callback);

  void CacheEmbeddings(const CoralRequest& request);

  // Creates a new desk for the content group from `source_desk`.
  void OpenNewDeskWithGroup(CoralResponse::Group group,
                            const Desk* source_desk);

  // Creates a saved desk with up to one browser with tabs from `group`.
  void CreateSavedDeskFromGroup(const std::string& template_name,
                                coral::mojom::GroupPtr group,
                                aura::Window* root_window);

  void OpenFeedbackDialog(const std::string& group_description);

 private:
  using CoralProcessor = coral::mojom::CoralProcessor;

  // Requests coral processor from service manager and returns the pointer of
  // the processor instance.
  CoralProcessor* EnsureCoralProcessor(std::string language);

  // Used as the callback of mojom::CoralProcessor::Group.
  void HandleGroupResult(CoralSource source,
                         CoralResponseCallback callback,
                         const base::TimeTicks& request_time,
                         coral::mojom::GroupResultPtr result);

  // Used as the callback of mojom::CoralProcessor::CacheEmbeddings.
  void HandleCacheEmbeddingsResult(
      coral::mojom::CacheEmbeddingsResultPtr result);

  // Callback that is run when we call
  // `DesksController::CaptureActiveDeskAsSavedDesk()`. May also be called
  // directly if a group has no apps in it. If `desk_template` is nullptr, then
  // we create one if `tab_urls` is not empty, otherwise this function does
  // nothing.
  void OnTemplateCreated(std::vector<coral::mojom::EntityPtr> tab_app_entities,
                         std::unique_ptr<aura::WindowTracker> window_tracker,
                         const std::string& template_name,
                         std::unique_ptr<DeskTemplate> desk_template);

  // Callback that is run after a saved desk is saved. Shows the saved desk
  // library if we are still in overview. `window_tracker` contains the root
  // window that the `BirchChipButton` context menu was clicked from. Default to
  // the primary root if the root was removed during the async callbacks for any
  // reason.
  void ShowSavedDeskLibrary(
      std::unique_ptr<aura::WindowTracker> window_tracker,
      desks_storage::DeskModel::AddOrUpdateEntryStatus status,
      std::unique_ptr<DeskTemplate> saved_desk);

  // Sends the feedback when the send button is clicked. The group info was
  // saved in the `feedback_info`.
  void OnFeedbackSendButtonClicked(ScannerFeedbackInfo feedback_info,
                                   const std::string& user_description);

  mojo::Remote<coral::mojom::CoralService> coral_service_;
  mojo::Remote<coral::mojom::CoralProcessor> coral_processor_;

  std::unique_ptr<FakeCoralProcessor> fake_processor_;

  base::WeakPtrFactory<CoralController> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_WM_CORAL_CORAL_CONTROLLER_H_
