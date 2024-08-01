// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>
#include <vector>

#include "base/memory/raw_ref.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/chromeos/extensions/file_system_provider/service_worker_lifetime_manager.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/service_worker/worker_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions::file_system_provider {

namespace {

using UuidSet = std::set<std::string>;
using KeepaliveMap = std::map<WorkerId, UuidSet>;

// Implementation of ServiceWorkerLifetimeManager that stubs out the calls to
// the non-existing ProcessManager in IncrementKeepalive/DecrementKeepalive.
class TestServiceWorkerLifetimeManager : public ServiceWorkerLifetimeManager {
 public:
  explicit TestServiceWorkerLifetimeManager(KeepaliveMap& keepalive_map)
      : ServiceWorkerLifetimeManager(nullptr), keepalive_map_(keepalive_map) {}

  void Reset() { next_keepalive_id_ = 1; }

 private:
  std::string IncrementKeepalive(const WorkerId& worker_id) override {
    std::string id = base::StringPrintf("uuid-%d", next_keepalive_id_++);
    (*keepalive_map_)[worker_id].insert(id);
    return id;
  }

  void DecrementKeepalive(const KeepaliveKey& key) override {
    DCHECK(base::Contains(*keepalive_map_, key.worker_id));
    DCHECK(base::Contains((*keepalive_map_)[key.worker_id], key.request_uuid));
    (*keepalive_map_)[key.worker_id].erase(key.request_uuid);
    if ((*keepalive_map_)[key.worker_id].empty()) {
      keepalive_map_->erase(key.worker_id);
    }
  }

  // Effectively emulates ProcessManager.
  int next_keepalive_id_ = 1;
  const raw_ref<KeepaliveMap> keepalive_map_;
};

}  // namespace

class ServiceWorkerLifetimeManagerTest : public testing::Test {
 public:
  void SetUp() override { sw_lifetime_manager_.Reset(); }

 protected:
  KeepaliveMap keepalive_map_;
  TestServiceWorkerLifetimeManager sw_lifetime_manager_{keepalive_map_};
};

TEST_F(ServiceWorkerLifetimeManagerTest, TestNoDispatch) {
  const RequestKey kRequest{
      .extension_id = "ext1",
      .file_system_id = "fs1",
      .request_id = 1,
  };

  // Keepalive count should not be incremented if a request is never dispatched.

  sw_lifetime_manager_.StartRequest(kRequest);
  EXPECT_EQ(keepalive_map_.size(), 0u);

  sw_lifetime_manager_.FinishRequest(kRequest);
  EXPECT_EQ(keepalive_map_.size(), 0u);
}

TEST_F(ServiceWorkerLifetimeManagerTest, TestDispatchOneTarget) {
  const RequestKey kRequest{
      .extension_id = "ext1",
      .file_system_id = "fs1",
      .request_id = 1,
  };
  const EventTarget kTarget{
      .extension_id = "ext1",
      .render_process_id = 1,
      .service_worker_version_id = 2,
      .worker_thread_id = 3,
  };
  const WorkerId kWorkerId(/*extension_id=*/"ext1", /*render_process_id=*/1,
                           /*version_id=*/2,
                           /*thread_id=*/3);

  // Simple case: a single request is dispatched and completed.

  sw_lifetime_manager_.StartRequest(kRequest);
  EXPECT_EQ(keepalive_map_.size(), 0u);

  sw_lifetime_manager_.RequestDispatched(kRequest, kTarget);
  EXPECT_EQ(keepalive_map_.size(), 1u);
  EXPECT_EQ(keepalive_map_[kWorkerId], UuidSet{"uuid-1"});

  sw_lifetime_manager_.FinishRequest(kRequest);
  EXPECT_EQ(keepalive_map_.size(), 0u);
}

TEST_F(ServiceWorkerLifetimeManagerTest, TestDispatchMultipleTargets) {
  const RequestKey kRequest{
      .extension_id = "ext1",
      .file_system_id = "fs1",
      .request_id = 1,
  };
  auto target = [](int version_id) {
    return EventTarget{
        .extension_id = "ext1",
        .render_process_id = 1000,
        .service_worker_version_id = version_id,
        .worker_thread_id = 3,
    };
  };
  auto worker = [](int version_id) {
    return WorkerId(
        /*extension_id=*/"ext1",
        /*render_process_id=*/1000, version_id,
        /*thread_id=*/3);
  };

  // A request is dispatched to multiple targets in the same extension.

  sw_lifetime_manager_.StartRequest(kRequest);
  EXPECT_EQ(keepalive_map_.size(), 0u);

  sw_lifetime_manager_.RequestDispatched(kRequest, target(1));
  EXPECT_EQ(keepalive_map_.size(), 1u);
  EXPECT_EQ(keepalive_map_[worker(1)], UuidSet{"uuid-1"});

  sw_lifetime_manager_.RequestDispatched(kRequest, target(2));
  EXPECT_EQ(keepalive_map_.size(), 2u);
  EXPECT_EQ(keepalive_map_[worker(1)], UuidSet{"uuid-1"});
  EXPECT_EQ(keepalive_map_[worker(2)], UuidSet{"uuid-2"});

  // Finishing a request clears out any keepalive references associated with
  // this request.

  sw_lifetime_manager_.FinishRequest(kRequest);
  EXPECT_EQ(keepalive_map_.size(), 0u);
}

TEST_F(ServiceWorkerLifetimeManagerTest, TestDispatchLate) {
  const RequestKey kRequest{
      .extension_id = "ext1",
      .file_system_id = "fs1",
      .request_id = 1,
  };

  sw_lifetime_manager_.StartRequest(kRequest);
  EXPECT_EQ(keepalive_map_.size(), 0u);

  sw_lifetime_manager_.FinishRequest(kRequest);
  EXPECT_EQ(keepalive_map_.size(), 0u);

  sw_lifetime_manager_.RequestDispatched(kRequest,
                                         EventTarget{
                                             .extension_id = "ext1",
                                             .render_process_id = 1,
                                             .service_worker_version_id = 2,
                                             .worker_thread_id = 3,
                                         });
  EXPECT_EQ(keepalive_map_.size(), 0u);
}

TEST_F(ServiceWorkerLifetimeManagerTest, TestDispatchMultipleEvents) {
  const EventTarget kTarget1{
      .extension_id = "ext1",
      .render_process_id = 1000,
      .service_worker_version_id = 2,
      .worker_thread_id = 3,
  };
  const EventTarget kTarget2{
      .extension_id = "ext2",
      .render_process_id = 1001,
      .service_worker_version_id = 4,
      .worker_thread_id = 5,
  };
  const WorkerId kWorkerId1(
      /*extension_id=*/"ext1",
      /*render_process_id=*/1000,
      /*version_id=*/2,
      /*thread_id=*/3);
  const WorkerId kWorkerId2(
      /*extension_id=*/"ext2",
      /*render_process_id=*/1001,
      /*version_id=*/4,
      /*thread_id=*/5);

  // Send four requests:
  // - two different extensions,
  // - two separate file system instances in the same extension,
  // - two requests to the same file system instance.
  sw_lifetime_manager_.StartRequest(RequestKey{"ext1", "fs1-1", 1});
  sw_lifetime_manager_.StartRequest(RequestKey{"ext1", "fs1-2", 1});
  sw_lifetime_manager_.StartRequest(RequestKey{"ext1", "fs1-1", 2});
  sw_lifetime_manager_.StartRequest(RequestKey{"ext2", "fs2-1", 1});
  sw_lifetime_manager_.RequestDispatched(RequestKey{"ext1", "fs1-1", 1},
                                         kTarget1);
  sw_lifetime_manager_.RequestDispatched(RequestKey{"ext1", "fs1-2", 1},
                                         kTarget1);
  sw_lifetime_manager_.RequestDispatched(RequestKey{"ext1", "fs1-1", 2},
                                         kTarget1);
  sw_lifetime_manager_.RequestDispatched(RequestKey{"ext2", "fs2-1", 1},
                                         kTarget2);

  // Three events incremented one service worker's keepalive three times, and
  // one event for another service worker.
  ASSERT_EQ(keepalive_map_.size(), 2u);
  UuidSet expectedWorker1{"uuid-1", "uuid-2", "uuid-3"};
  UuidSet expectedWorker2{"uuid-4"};
  EXPECT_EQ(keepalive_map_[kWorkerId1], expectedWorker1);
  EXPECT_EQ(keepalive_map_[kWorkerId2], expectedWorker2);

  // As requests finish, the worker's keepalive references keep getting removed
  // until none left.

  sw_lifetime_manager_.FinishRequest(RequestKey{"ext1", "fs1-1", 1});

  ASSERT_EQ(keepalive_map_.size(), 2u);
  expectedWorker1 = {"uuid-2", "uuid-3"};
  EXPECT_EQ(keepalive_map_[kWorkerId1], expectedWorker1);
  EXPECT_EQ(keepalive_map_[kWorkerId2], expectedWorker2);

  sw_lifetime_manager_.FinishRequest(RequestKey{"ext1", "fs1-1", 2});

  ASSERT_EQ(keepalive_map_.size(), 2u);
  expectedWorker1 = {"uuid-2"};
  EXPECT_EQ(keepalive_map_[kWorkerId1], expectedWorker1);
  EXPECT_EQ(keepalive_map_[kWorkerId2], expectedWorker2);

  sw_lifetime_manager_.FinishRequest(RequestKey{"ext1", "fs1-2", 1});

  ASSERT_EQ(keepalive_map_.size(), 1u);
  EXPECT_EQ(keepalive_map_[kWorkerId2], expectedWorker2);

  sw_lifetime_manager_.FinishRequest(RequestKey{"ext2", "fs2-1", 1});

  ASSERT_EQ(keepalive_map_.size(), 0u);
}

}  // namespace extensions::file_system_provider
