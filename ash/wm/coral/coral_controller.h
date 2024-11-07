// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_CORAL_CORAL_CONTROLLER_H_
#define ASH_WM_CORAL_CORAL_CONTROLLER_H_

#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ash/birch/coral_constants.h"
#include "base/memory/weak_ptr.h"
#include "base/token.h"
#include "chromeos/ash/services/coral/public/mojom/coral_service.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {

class FakeCoralService;

class ASH_EXPORT CoralRequest {
 public:
  using ContentItem = coral::mojom::EntityPtr;

  CoralRequest();
  CoralRequest(const CoralRequest&) = delete;
  CoralRequest& operator=(const CoralRequest&) = delete;
  ~CoralRequest();

  void set_source(CoralSource source) { source_ = source; }

  void set_content(std::vector<ContentItem>&& content) {
    content_ = std::move(content);
  }

  CoralSource source() const { return source_; }

  const std::vector<ContentItem>& content() const { return content_; }

  std::string ToString() const;

 private:
  CoralSource source_ = CoralSource::kUnknown;

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
  // `GenerateContentGroups` and `CacheEmbeddings` requests. It is not necessary
  // to call `PrepareResource` before calling other methods, but in that case
  // the first method request might take longer to run.
  void PrepareResource();

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

  // Callback returns whether the request was successful.
  void CacheEmbeddings(const CoralRequest& request,
                       base::OnceCallback<void(bool)> callback);

  // Creates a new desk for the content group.
  void OpenNewDeskWithGroup(CoralResponse::Group group);

 private:
  using CoralService = coral::mojom::CoralService;

  // Requests coral service from service manager and returns the pointer of the
  // service instance.
  CoralService* EnsureCoralService();

  // Used as the callback of mojom::CoralService::Group.
  void HandleGroupResult(CoralSource source,
                         CoralResponseCallback callback,
                         const base::TimeTicks& request_time,
                         coral::mojom::GroupResultPtr result);

  // Used as the callback of mojom::CoralService::CacheEmbeddings. `callback` is
  // the callback passed from `CoralController::CacheEmbeddings`, which should
  // be triggered with a bool indicating whether the CacheEmbeddings operation
  // was successful.
  void HandleCacheEmbeddingsResult(
      base::OnceCallback<void(bool)> callback,
      coral::mojom::CacheEmbeddingsResultPtr result);

  mojo::Remote<CoralService> coral_service_;

  std::unique_ptr<FakeCoralService> fake_service_;

  base::WeakPtrFactory<CoralController> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_WM_CORAL_CORAL_CONTROLLER_H_
