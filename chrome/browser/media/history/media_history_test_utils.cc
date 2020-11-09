// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/history/media_history_test_utils.h"

#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/media/history/media_history_keyed_service.h"

namespace media_history {
namespace test {

base::Optional<base::UnguessableToken> GetResetTokenSync(
    MediaHistoryKeyedService* service,
    const int64_t feed_id) {
  base::RunLoop run_loop;
  base::Optional<base::UnguessableToken> out;

  service->GetMediaFeedFetchDetails(
      feed_id,
      base::BindLambdaForTesting(
          [&](base::Optional<MediaHistoryKeyedService::MediaFeedFetchDetails>
                  details) {
            if (details)
              out = details->reset_token;
            run_loop.Quit();
          }));

  run_loop.Run();
  return out;
}

}  // namespace test
}  // namespace media_history
