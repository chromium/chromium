// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_BIRCH_BIRCH_CORAL_PROVIDER_H_
#define ASH_BIRCH_BIRCH_CORAL_PROVIDER_H_

#include "ash/ash_export.h"
#include "ash/birch/birch_data_provider.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/public/cpp/tab_cluster/tab_cluster_ui_controller.h"
#include "ash/wm/coral/coral_controller.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_observer.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "base/token.h"
#include "chromeos/ash/services/coral/public/mojom/coral_service.mojom.h"
#include "ui/aura/window_observer.h"

namespace ash {

class BirchModel;
class CoralItemRemover;

class ASH_EXPORT BirchCoralProvider : public BirchDataProvider,
                                      public TabClusterUIController::Observer,
                                      public coral::mojom::TitleObserver,
                                      public SessionObserver,
                                      public aura::WindowObserver,
                                      public OverviewObserver {
 public:
  explicit BirchCoralProvider(BirchModel* birch_model);
  BirchCoralProvider(const BirchCoralProvider&) = delete;
  BirchCoralProvider& operator=(const BirchCoralProvider&) = delete;
  ~BirchCoralProvider() override;

  static BirchCoralProvider* Get();

  // Gets a group reference with given group ID. This operation will not remove
  // the group from the `response_`.
  const coral::mojom::GroupPtr& GetGroupById(const base::Token& group_id) const;

  // Extracts a group from the response with given group ID. This operation will
  // remove the group from the `response_`.
  coral::mojom::GroupPtr ExtractGroupById(const base::Token& group_id);

  // Removes the group with `group_id` from the `response_` and adds all items
  // in the group to the coral item remover blocklist.
  void RemoveGroup(const base::Token& group_id);

  // Removes an item with `identifier` from the group with `group_id`.
  void RemoveItemFromGroup(const base::Token& group_id,
                           const std::string& identifier);

  void OnPostLoginClusterRestored();

  mojo::PendingRemote<coral::mojom::TitleObserver> BindRemote();

  // BirchDataProvider:
  void RequestBirchDataFetch() override;

  // TabClusterUIController::Observer:
  void OnTabItemAdded(TabClusterUIItem* tab_item) override;
  void OnTabItemUpdated(TabClusterUIItem* tab_item) override;
  void OnTabItemRemoved(TabClusterUIItem* tab_item) override;

  // coral::mojom::TitleObserver:
  void TitleUpdated(const base::Token& id, const std::string& title) override;

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;

  // aura::WindowObserver:
  void OnWindowDestroyed(aura::Window* window) override;
  void OnWindowParentChanged(aura::Window* window,
                             aura::Window* parent) override;

  // OverviewObserver:
  void OnOverviewModeEnded() override;

  const CoralRequest& GetCoralRequestForTest() const { return request_; }

  CoralItemRemover* GetCoralItemRemoverForTest() {
    return coral_item_remover_.get();
  }

  void OverrideCoralResponseForTest(std::unique_ptr<CoralResponse> response);

 private:
  // Whether we should handle post-login or in-session data.
  bool HasValidPostLoginData() const;

  // Called during session restore. Sends a grouping request with session
  // restore data to the coral backend.
  void HandlePostLoginDataRequest();

  // Called during user session. Sends a grouping request with active tab
  // and app metadata to the coral backend.
  void HandleInSessionDataRequest();

  // Checks whether we have fresh post-login data.
  bool HasValidPostLoginResponse();

  // Callback passed to the coral backend when performing post login clustering.
  void HandlePostLoginCoralResponse(std::unique_ptr<CoralResponse> response);

  // Callback passed to the coral backend when performing in-session custering.
  void HandleInSessionCoralResponse(std::unique_ptr<CoralResponse> response);

  // Handles responses from coral backend.
  void HandleCoralResponse(std::unique_ptr<CoralResponse> response);

  // Erases from the ContentItem list any items which have been removed by the
  // user. The list is mutated in place.
  void FilterCoralContentItems(std::vector<coral::mojom::EntityPtr>* items);

  // Only cache embeddings for valid tabs/windows.
  void MaybeCacheTabEmbedding(TabClusterUIItem* tab_item);

  // Sends a request to the coral backend to cache the embedding for `tab_item`.
  void CacheTabEmbedding(TabClusterUIItem* tab_item);

  void HandleEmbeddingResult(bool success);

  // Observes all the valid app and browser windows associated with `response_`.
  void ObserveAllWindowsInResponse();

  // Called when the `tab_item` is removed or moved to another inactive desk.
  void OnTabRemovedFromActiveDesk(TabClusterUIItem* tab_item);

  // Called when an `app_window` is removed or moved to another inactive desk.
  void OnAppWindowRemovedFromActiveDesk(aura::Window* app_window);

  // Removes the entity corresponding to the given `entity_identifier` from
  // current in-session `response_`.
  void RemoveEntity(std::string_view entity_identifier);

  const raw_ptr<BirchModel> birch_model_;

  // The request sent to the coral backend.
  CoralRequest request_;

  // Timestamp for when post login coral response expires.
  base::TimeTicks post_login_response_expiration_timestamp_;

  // Response generated by the coral backend.
  std::unique_ptr<CoralResponse> response_;

  // Take fake response for test using.
  std::unique_ptr<CoralResponse> fake_response_;

  // Used to filter out coral items which have been removed by the user in
  // the current session.
  std::unique_ptr<CoralItemRemover> coral_item_remover_;

  mojo::Receiver<coral::mojom::TitleObserver> receiver_{this};

  ScopedSessionObserver session_observer_{this};

  base::ScopedMultiSourceObservation<aura::Window, aura::WindowObserver>
      windows_observation_{this};

  base::ScopedObservation<OverviewController, OverviewObserver>
      overview_observation_{this};

  base::WeakPtrFactory<BirchCoralProvider> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_BIRCH_BIRCH_CORAL_PROVIDER_H_
