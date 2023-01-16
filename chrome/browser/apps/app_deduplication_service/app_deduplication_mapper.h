// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_DEDUPLICATION_SERVICE_APP_DEDUPLICATION_MAPPER_H_
#define CHROME_BROWSER_APPS_APP_DEDUPLICATION_SERVICE_APP_DEDUPLICATION_MAPPER_H_

#include "chrome/browser/apps/app_deduplication_service/proto/app_deduplication.pb.h"
#include "chrome/browser/apps/app_deduplication_service/proto/deduplication_data.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace apps::deduplication {

// The AppDeduplicationMapper is used to map between the DeduplicateResponse
// proto returned from the deduplication endpoint in the Fondue server and
// the DeduplicateData proto.
class AppDeduplicationMapper {
 public:
  AppDeduplicationMapper();
  AppDeduplicationMapper(const AppDeduplicationMapper&) = delete;
  AppDeduplicationMapper& operator=(const AppDeduplicationMapper&) = delete;
  ~AppDeduplicationMapper();

  // Maps the deduplicate response proto to the deduplicate data proto.
  absl::optional<proto::DeduplicateData> ToDeduplicateData(
      const proto::DeduplicateResponse& response);
};

}  // namespace apps::deduplication

#endif  // CHROME_BROWSER_APPS_APP_DEDUPLICATION_SERVICE_APP_DEDUPLICATION_MAPPER_H_
