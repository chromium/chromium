// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/ttc_actor_task_delegate.h"

#include <utility>

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/common/actor_webui.mojom.h"
#include "components/actor/core/task_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace ttc {
namespace {

namespace webui_mojom = actor::webui::mojom;

class TtcActorTaskDelegateTest : public testing::Test {
 protected:
  const actor::TaskId kTaskId = actor::TaskId(42);
  const url::Origin kOrigin = url::Origin::Create(GURL("https://example.com/"));

  base::test::TaskEnvironment task_environment_;
  TtcActorTaskDelegate delegate_;
};

TEST_F(TtcActorTaskDelegateTest, GrantsNavigationConfirmation) {
  base::test::TestFuture<webui_mojom::NavigationConfirmationResponsePtr> future;
  delegate_.RequestToConfirmNavigation(kTaskId, kOrigin, future.GetCallback());

  webui_mojom::NavigationConfirmationResponsePtr response = future.Take();
  ASSERT_TRUE(response->result->is_permission_granted());
  EXPECT_TRUE(response->result->get_permission_granted());
}

TEST_F(TtcActorTaskDelegateTest, DeclinesUserConfirmationDialog) {
  base::test::TestFuture<webui_mojom::UserConfirmationDialogResponsePtr> future;
  delegate_.RequestToShowUserConfirmationDialog(
      kTaskId, kOrigin, /*for_blocklisted_origin=*/false, future.GetCallback());

  webui_mojom::UserConfirmationDialogResponsePtr response = future.Take();
  ASSERT_TRUE(response->result->is_error_reason());
  EXPECT_EQ(
      response->result->get_error_reason(),
      webui_mojom::ConfirmationRequestErrorReason::kRequestPromiseNoSubscriber);
}

}  // namespace
}  // namespace ttc
