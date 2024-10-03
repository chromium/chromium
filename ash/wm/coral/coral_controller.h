// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_CORAL_CORAL_CONTROLLER_H_
#define ASH_WM_CORAL_CORAL_CONTROLLER_H_

#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/services/coral/public/mojom/coral_service.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {

class ASH_EXPORT CoralRequest {
 public:
  using ContentItem = coral::mojom::EntityPtr;

  CoralRequest();
  CoralRequest(const CoralRequest&) = delete;
  CoralRequest& operator=(const CoralRequest&) = delete;
  ~CoralRequest();

  void set_content(std::vector<ContentItem>&& content) {
    content_ = std::move(content);
  }

  const std::vector<ContentItem>& content() const { return content_; }

  std::string ToString() const;

 private:
  // Tab/app content with arbitrary ordering.
  std::vector<ContentItem> content_;
};

// `CoralResponse` contains 0-2 groups in order of relevance.
class ASH_EXPORT CoralResponse {
 public:
  using Group = coral::mojom::GroupPtr;

  CoralResponse();
  CoralResponse(const CoralResponse&) = delete;
  CoralResponse& operator=(const CoralResponse&) = delete;
  ~CoralResponse();

  void set_groups(std::vector<Group>&& groups) { groups_ = std::move(groups); }

  const std::vector<Group>& groups() const { return groups_; }

 private:
  std::vector<Group> groups_;
};

// Controller interface of the coral feature.
class ASH_EXPORT CoralController {
 public:
  CoralController();
  CoralController(const CoralController&) = delete;
  CoralController& operator=(const CoralController&) = delete;
  ~CoralController();

  // GenerateContentGroups clusters the input ContentItems (which includes web
  // tabs, apps, etc.) into suitable groups based on their topics, and gives
  // each group a suitable title. If GenerateContentGroups request failed,
  // nullptr will be returned.
  using CoralResponseCallback =
      base::OnceCallback<void(std::unique_ptr<CoralResponse>)>;
  void GenerateContentGroups(const CoralRequest& request,
                             CoralResponseCallback callback);

  // Callback returns whether the request was successful.
  void CacheEmbeddings(const CoralRequest& request,
                       base::OnceCallback<void(bool)> callback);

  // Creates a new desk for the content group.
  void OpenNewDeskWithGroup(CoralResponse::Group group);

 private:
  using CoralService = coral::mojom::CoralService;

  void EnsureCoralService();

  // Used as the callback of mojom::CoralService::Group.
  void HandleGroupResult(CoralResponseCallback callback,
                         coral::mojom::GroupResultPtr result);

  // Used as the callback of mojom::CoralService::CacheEmbeddings. `callback` is
  // the callback passed from `CoralController::CacheEmbeddings`, which should
  // be triggered with a bool indicating whether the CacheEmbeddings operation
  // was successful.
  void HandleCacheEmbeddingsResult(
      base::OnceCallback<void(bool)> callback,
      coral::mojom::CacheEmbeddingsResultPtr result);

  mojo::Remote<CoralService> coral_service_;

  base::WeakPtrFactory<CoralController> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_WM_CORAL_CORAL_CONTROLLER_H_
