// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_permission_manager.h"

#include <list>
#include <memory>

#include "android_webview/browser/aw_browser_permission_request_delegate.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_type.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using blink::mojom::PermissionStatus;
using content::PermissionType;

namespace android_webview {

namespace {

int kNoPendingOperation = -1;

int kRenderProcessIDForTesting = 8;
int kRenderFrameIDForTesting = 19;
const char kEmbeddingOrigin[] = "https://www.google.com/";
const char kRequestingOrigin1[] = "https://www.google.com/";
const char kRequestingOrigin2[] = "https://www.chromium.org/";

class AwBrowserPermissionRequestDelegateForTesting final
    : public AwBrowserPermissionRequestDelegate {
 public:
  void EnqueueResponse(const std::string& origin,
                       PermissionType type,
                       bool grant) {
    for (auto it = request_.begin(); it != request_.end(); ++it) {
      if ((*it)->type != type || (*it)->origin != origin)
        continue;
      base::OnceCallback<void(bool)> callback = std::move((*it)->callback);
      request_.erase(it);
      std::move(callback).Run(grant);
      return;
    }
    response_.push_back(std::make_unique<Response>(origin, type, grant));
  }

  // AwBrowserPermissionRequestDelegate:
  void RequestProtectedMediaIdentifierPermission(
      const GURL& origin,
      base::OnceCallback<void(bool)> callback) override {}

  void CancelProtectedMediaIdentifierPermissionRequests(
      const GURL& origin) override {}

  void RequestGeolocationPermission(
      const GURL& origin,
      base::OnceCallback<void(bool)> callback) override {
    RequestPermission(origin, PermissionType::GEOLOCATION, std::move(callback));
  }

  void CancelGeolocationPermissionRequests(const GURL& origin) override {
    CancelPermission(origin, PermissionType::GEOLOCATION);
  }

  void RequestMIDISysexPermission(
      const GURL& origin,
      base::OnceCallback<void(bool)> callback) override {
    RequestPermission(origin, PermissionType::MIDI_SYSEX, std::move(callback));
  }

  void CancelMIDISysexPermissionRequests(const GURL& origin) override {
    CancelPermission(origin, PermissionType::MIDI_SYSEX);
  }

 private:
  void RequestPermission(const GURL& origin,
                         PermissionType type,
                         base::OnceCallback<void(bool)> callback) {
    for (auto it = response_.begin(); it != response_.end(); ++it) {
      if ((*it)->type != type || (*it)->origin != origin)
        continue;
      bool grant = (*it)->grant;
      response_.erase(it);
      std::move(callback).Run(grant);
      return;
    }
    request_.push_back(
        std::make_unique<Request>(origin, type, std::move(callback)));
  }

  void CancelPermission(const GURL& origin, PermissionType type) {
    for (auto it = request_.begin(); it != request_.end(); ++it) {
      if ((*it)->type != type || (*it)->origin != origin)
        continue;
      request_.erase(it);
      return;
    }
    NOTREACHED();
  }

 private:
  struct Request {
    GURL origin;
    PermissionType type;
    base::OnceCallback<void(bool)> callback;

    Request(const GURL& origin,
            PermissionType type,
            base::OnceCallback<void(bool)> callback)
        : origin(origin), type(type), callback(std::move(callback)) {}
  };

  struct Response {
    GURL origin;
    PermissionType type;
    bool grant;

    Response(const std::string& origin, PermissionType type, bool grant)
        : origin(GURL(origin)), type(type), grant(grant) {}

  };

  std::list<std::unique_ptr<Response>> response_;
  std::list<std::unique_ptr<Request>> request_;
};

class AwPermissionManagerForTesting : public AwPermissionManager {
 public:
  ~AwPermissionManagerForTesting() override {
    // Call CancelPermissionRequests() from here so that it calls virtual
    // methods correctly.
    CancelPermissionRequests();
  }

  void EnqueuePermissionResponse(const std::string& origin,
                                 PermissionType type,
                                 bool grant) {
    delegate()->EnqueueResponse(origin, type, grant);
  }

 private:
  AwBrowserPermissionRequestDelegateForTesting* delegate() {
    if (!delegate_)
      delegate_.reset(new AwBrowserPermissionRequestDelegateForTesting);
    return delegate_.get();
  }

  // AwPermissionManager:
  int GetRenderProcessID(content::RenderFrameHost* render_frame_host) override {
    return kRenderProcessIDForTesting;
  }

  int GetRenderFrameID(content::RenderFrameHost* render_frame_host) override {
    return kRenderFrameIDForTesting;
  }

  GURL LastCommittedOrigin(
      content::RenderFrameHost* render_frame_host) override {
    return GURL(kEmbeddingOrigin);
  }

  AwBrowserPermissionRequestDelegate* GetDelegate(
      int render_process_id,
      int render_frame_id) override {
    CHECK_EQ(kRenderProcessIDForTesting, render_process_id);
    CHECK_EQ(kRenderFrameIDForTesting, render_frame_id);
    return delegate();
  }

  std::unique_ptr<AwBrowserPermissionRequestDelegateForTesting> delegate_;
};

class AwPermissionManagerTest : public testing::Test {
 public:
  AwPermissionManagerTest()
      : render_frame_host(nullptr) {}

  void PermissionRequestResponse(int id, const PermissionStatus status) {
    resolved_permission_status.push_back(status);
    resolved_permission_request_id.push_back(id);
  }

  void PermissionsRequestResponse(int id,
                                  const std::vector<PermissionStatus>& status) {
    resolved_permission_status.insert(resolved_permission_status.end(),
                                      status.begin(), status.end());
    for (size_t i = 0; i < status.size(); ++i)
      resolved_permission_request_id.push_back(id);
  }

 protected:
  void SetUp() override { manager.reset(new AwPermissionManagerForTesting); }
  void TearDown() override { manager.reset(); }

  void EnqueuePermissionResponse(const std::string& origin,
                                 PermissionType type,
                                 bool grant) {
    CHECK(manager);
    manager->EnqueuePermissionResponse(origin, type, grant);
  }

  std::unique_ptr<AwPermissionManagerForTesting> manager;

  // Use nullptr for testing. AwPermissionManagerForTesting override all methods
  // that touch RenderFrameHost to work with nullptr.
  content::RenderFrameHost* render_frame_host;

  std::vector<PermissionStatus> resolved_permission_status;
  std::vector<int> resolved_permission_request_id;
};

// The most simple test, PermissionType::MIDI is hard-coded to be granted.
TEST_F(AwPermissionManagerTest, MIDIPermissionIsGrantedSynchronously) {
  int request_id = manager->RequestPermission(
      PermissionType::MIDI, render_frame_host, GURL(kRequestingOrigin1), true,
      base::BindOnce(&AwPermissionManagerTest::PermissionRequestResponse,
                     base::Unretained(this), 0));
  EXPECT_EQ(kNoPendingOperation, request_id);
  ASSERT_EQ(1u, resolved_permission_status.size());
  EXPECT_EQ(PermissionStatus::GRANTED, resolved_permission_status[0]);
}

// Test the case a delegate is called, and it resolves the permission
// synchronously.
TEST_F(AwPermissionManagerTest, SinglePermissionRequestIsGrantedSynchronously) {
  // Permission should be granted in this scenario.
  manager->EnqueuePermissionResponse(kRequestingOrigin1,
                                     PermissionType::GEOLOCATION, true);
  int request_id = manager->RequestPermission(
      PermissionType::GEOLOCATION, render_frame_host, GURL(kRequestingOrigin1),
      true,
      base::BindOnce(&AwPermissionManagerTest::PermissionRequestResponse,
                     base::Unretained(this), 0));
  EXPECT_EQ(kNoPendingOperation, request_id);
  ASSERT_EQ(1u, resolved_permission_status.size());
  EXPECT_EQ(PermissionStatus::GRANTED, resolved_permission_status[0]);

  // Permission should not be granted in this scenario.
  manager->EnqueuePermissionResponse(kRequestingOrigin1,
                                     PermissionType::GEOLOCATION, false);
  request_id = manager->RequestPermission(
      PermissionType::GEOLOCATION, render_frame_host, GURL(kRequestingOrigin1),
      true,
      base::BindOnce(&AwPermissionManagerTest::PermissionRequestResponse,
                     base::Unretained(this), 0));
  EXPECT_EQ(kNoPendingOperation, request_id);
  ASSERT_EQ(2u, resolved_permission_status.size());
  EXPECT_EQ(PermissionStatus::DENIED, resolved_permission_status[1]);
}

// Test the case a delegate is called, and it resolves the permission
// asynchronously.
TEST_F(AwPermissionManagerTest,
       SinglePermissionRequestIsGrantedAsynchronously) {
  int request_id = manager->RequestPermission(
      PermissionType::GEOLOCATION, render_frame_host, GURL(kRequestingOrigin1),
      true,
      base::BindOnce(&AwPermissionManagerTest::PermissionRequestResponse,
                     base::Unretained(this), 0));
  EXPECT_NE(kNoPendingOperation, request_id);
  EXPECT_EQ(0u, resolved_permission_status.size());

  // This will resolve the permission.
  manager->EnqueuePermissionResponse(kRequestingOrigin1,
                                     PermissionType::GEOLOCATION, true);

  ASSERT_EQ(1u, resolved_permission_status.size());
  EXPECT_EQ(PermissionStatus::GRANTED, resolved_permission_status[0]);
}

// Test the case a delegate is called, and the manager is deleted before the
// delegate callback is invoked.
TEST_F(AwPermissionManagerTest, ManagerIsDeletedWhileDelegateProcesses) {
  int request_id = manager->RequestPermission(
      PermissionType::GEOLOCATION, render_frame_host, GURL(kRequestingOrigin1),
      true,
      base::BindOnce(&AwPermissionManagerTest::PermissionRequestResponse,
                     base::Unretained(this), 0));

  EXPECT_NE(kNoPendingOperation, request_id);
  EXPECT_EQ(0u, resolved_permission_status.size());

  // Delete the manager.
  manager.reset();

  // All requests are cancelled internally.
  EXPECT_EQ(0u, resolved_permission_status.size());
}

// Test the case multiple permissions are requested for the same origin, and the
// second permission is also resolved when the first permission is resolved.
TEST_F(AwPermissionManagerTest,
       MultiplePermissionRequestsAreGrantedTogether) {
  int request_1 = manager->RequestPermission(
      PermissionType::GEOLOCATION, render_frame_host, GURL(kRequestingOrigin1),
      true,
      base::BindOnce(&AwPermissionManagerTest::PermissionRequestResponse,
                     base::Unretained(this), 1));
  EXPECT_NE(kNoPendingOperation, request_1);

  int request_2 = manager->RequestPermission(
      PermissionType::GEOLOCATION, render_frame_host, GURL(kRequestingOrigin1),
      true,
      base::BindOnce(&AwPermissionManagerTest::PermissionRequestResponse,
                     base::Unretained(this), 2));
  EXPECT_NE(kNoPendingOperation, request_2);

  EXPECT_NE(request_1, request_2);
  EXPECT_EQ(0u, resolved_permission_status.size());

  // This will resolve the permission.
  manager->EnqueuePermissionResponse(kRequestingOrigin1,
                                     PermissionType::GEOLOCATION, true);

  ASSERT_EQ(2u, resolved_permission_status.size());
  EXPECT_EQ(PermissionStatus::GRANTED, resolved_permission_status[0]);
  EXPECT_EQ(PermissionStatus::GRANTED, resolved_permission_status[1]);
}

// Test the case multiple permissions are requested for different origins, and
// each permission is resolved respectively in the requested order.
TEST_F(AwPermissionManagerTest,
       MultiplePermissionRequestsAreGrantedRespectively) {
  int request_1 = manager->RequestPermission(
      PermissionType::GEOLOCATION, render_frame_host, GURL(kRequestingOrigin1),
      true,
      base::BindOnce(&AwPermissionManagerTest::PermissionRequestResponse,
                     base::Unretained(this), 1));
  EXPECT_NE(kNoPendingOperation, request_1);

  int request_2 = manager->RequestPermission(
      PermissionType::GEOLOCATION, render_frame_host, GURL(kRequestingOrigin2),
      true,
      base::BindOnce(&AwPermissionManagerTest::PermissionRequestResponse,
                     base::Unretained(this), 2));
  EXPECT_NE(kNoPendingOperation, request_2);

  EXPECT_NE(request_1, request_2);
  EXPECT_EQ(0u, resolved_permission_status.size());

  // This will resolve the first request.
  manager->EnqueuePermissionResponse(kRequestingOrigin1,
                                     PermissionType::GEOLOCATION, true);

  ASSERT_EQ(1u, resolved_permission_status.size());
  EXPECT_EQ(PermissionStatus::GRANTED, resolved_permission_status[0]);
  EXPECT_EQ(1, resolved_permission_request_id[0]);

  // This will resolve the second request.
  manager->EnqueuePermissionResponse(kRequestingOrigin2,
                                     PermissionType::GEOLOCATION, false);
  EXPECT_EQ(PermissionStatus::DENIED, resolved_permission_status[1]);
  EXPECT_EQ(2, resolved_permission_request_id[1]);
}

// Test the case multiple permissions are requested through single
// RequestPermissions call, then resolved synchronously.
TEST_F(AwPermissionManagerTest,
       SinglePermissionsRequestIsGrantedSynchronously) {
  manager->EnqueuePermissionResponse(kRequestingOrigin1,
                                     PermissionType::MIDI_SYSEX, false);

  std::vector<PermissionType> permissions = {PermissionType::MIDI,
                                             PermissionType::MIDI_SYSEX};

  int request_id = manager->RequestPermissions(
      permissions, render_frame_host, GURL(kRequestingOrigin1), true,
      base::BindOnce(&AwPermissionManagerTest::PermissionsRequestResponse,
                     base::Unretained(this), 0));
  EXPECT_EQ(kNoPendingOperation, request_id);

  ASSERT_EQ(2u, resolved_permission_status.size());
  EXPECT_EQ(PermissionStatus::GRANTED, resolved_permission_status[0]);
  EXPECT_EQ(PermissionStatus::DENIED, resolved_permission_status[1]);
}

// Test the case multiple permissions are requested through single
// RequestPermissions call, then one is resolved synchronously, the other is
// resolved asynchronously.
TEST_F(AwPermissionManagerTest,
       SinglePermissionsRequestIsGrantedAsynchronously) {
  std::vector<PermissionType> permissions = {PermissionType::MIDI,
                                             PermissionType::MIDI_SYSEX};

  int request_id = manager->RequestPermissions(
      permissions, render_frame_host, GURL(kRequestingOrigin1), true,
      base::BindOnce(&AwPermissionManagerTest::PermissionsRequestResponse,
                     base::Unretained(this), 0));
  EXPECT_NE(kNoPendingOperation, request_id);

  // PermissionType::MIDI is resolved synchronously, but all permissions result
  // are notified together when all permissions are resolved.
  EXPECT_EQ(0u, resolved_permission_status.size());

  manager->EnqueuePermissionResponse(kRequestingOrigin1,
                                     PermissionType::MIDI_SYSEX, false);

  ASSERT_EQ(2u, resolved_permission_status.size());
  EXPECT_EQ(PermissionStatus::GRANTED, resolved_permission_status[0]);
  EXPECT_EQ(PermissionStatus::DENIED, resolved_permission_status[1]);
}

// Test the case multiple permissions are requested multiple times as follow.
//  1. Permission A and B are requested.
//  2. Permission A is resolved.
//  3. Permission A is requested for the same origin before the B is resolved.
TEST_F(AwPermissionManagerTest, ComplicatedRequestScenario1) {
  // In the first case, the permission A is a type that does not call an
  // internal delegate method.
  std::vector<PermissionType> permissions_1 = {PermissionType::MIDI,
                                               PermissionType::MIDI_SYSEX};

  int request_1 = manager->RequestPermissions(
      permissions_1, render_frame_host, GURL(kRequestingOrigin1), true,
      base::BindOnce(&AwPermissionManagerTest::PermissionsRequestResponse,
                     base::Unretained(this), 1));
  EXPECT_NE(kNoPendingOperation, request_1);
  EXPECT_EQ(0u, resolved_permission_status.size());

  int request_2 = manager->RequestPermission(
      PermissionType::MIDI, render_frame_host, GURL(kRequestingOrigin1), true,
      base::BindOnce(&AwPermissionManagerTest::PermissionRequestResponse,
                     base::Unretained(this), 2));
  EXPECT_EQ(kNoPendingOperation, request_2);
  ASSERT_EQ(1u, resolved_permission_status.size());
  EXPECT_EQ(PermissionStatus::GRANTED, resolved_permission_status[0]);
  EXPECT_EQ(2, resolved_permission_request_id[0]);

  manager->EnqueuePermissionResponse(kRequestingOrigin1,
                                     PermissionType::MIDI_SYSEX, true);

  ASSERT_EQ(3u, resolved_permission_status.size());
  EXPECT_EQ(PermissionStatus::GRANTED, resolved_permission_status[1]);
  EXPECT_EQ(1, resolved_permission_request_id[1]);
  EXPECT_EQ(PermissionStatus::GRANTED, resolved_permission_status[2]);
  EXPECT_EQ(1, resolved_permission_request_id[2]);

  // In the second case, the permission A is a type that calls an internal
  // delegate method.
  std::vector<PermissionType> permissions_2 = {PermissionType::GEOLOCATION,
                                               PermissionType::MIDI_SYSEX};

  int request_3 = manager->RequestPermissions(
      permissions_2, render_frame_host, GURL(kRequestingOrigin1), true,
      base::BindOnce(&AwPermissionManagerTest::PermissionsRequestResponse,
                     base::Unretained(this), 3));
  EXPECT_NE(kNoPendingOperation, request_3);
  ASSERT_EQ(3u, resolved_permission_status.size());

  // The permission A is resolved, but the first request isn't finished.
  manager->EnqueuePermissionResponse(kRequestingOrigin1,
                                     PermissionType::GEOLOCATION, false);
  ASSERT_EQ(3u, resolved_permission_status.size());

  int request_4 = manager->RequestPermission(
      PermissionType::GEOLOCATION, render_frame_host, GURL(kRequestingOrigin1),
      true,
      base::BindOnce(&AwPermissionManagerTest::PermissionRequestResponse,
                     base::Unretained(this), 4));
  // The second request is finished first by using the resolved result for the
  // first request.
  EXPECT_EQ(kNoPendingOperation, request_4);
  ASSERT_EQ(4u, resolved_permission_status.size());
  EXPECT_EQ(PermissionStatus::DENIED, resolved_permission_status[3]);
  EXPECT_EQ(4, resolved_permission_request_id[3]);

  // Then the first request is finished.
  manager->EnqueuePermissionResponse(kRequestingOrigin1,
                                     PermissionType::MIDI_SYSEX, true);

  ASSERT_EQ(6u, resolved_permission_status.size());
  EXPECT_EQ(PermissionStatus::DENIED, resolved_permission_status[4]);
  EXPECT_EQ(3, resolved_permission_request_id[4]);
  EXPECT_EQ(PermissionStatus::GRANTED, resolved_permission_status[5]);
  EXPECT_EQ(3, resolved_permission_request_id[5]);
}

// Test the case multiple permissions are requested multiple times as follow.
//  1. Permission A and B are requested.
//  2. Permission A is resolved.
//  3. Permission A is requested for a different origin before the B is
//     resolved.
TEST_F(AwPermissionManagerTest, ComplicatedRequestScenario2) {
  // In the first case, the permission A is a type that does not call an
  // internal delegate method.
  std::vector<PermissionType> permissions_1 = {PermissionType::MIDI,
                                               PermissionType::MIDI_SYSEX};

  int request_1 = manager->RequestPermissions(
      permissions_1, render_frame_host, GURL(kRequestingOrigin1), true,
      base::BindOnce(&AwPermissionManagerTest::PermissionsRequestResponse,
                     base::Unretained(this), 1));
  EXPECT_NE(kNoPendingOperation, request_1);
  EXPECT_EQ(0u, resolved_permission_status.size());

  int request_2 = manager->RequestPermission(
      PermissionType::MIDI, render_frame_host, GURL(kRequestingOrigin2), true,
      base::BindOnce(&AwPermissionManagerTest::PermissionRequestResponse,
                     base::Unretained(this), 2));
  EXPECT_EQ(kNoPendingOperation, request_2);
  ASSERT_EQ(1u, resolved_permission_status.size());
  EXPECT_EQ(PermissionStatus::GRANTED, resolved_permission_status[0]);
  EXPECT_EQ(2, resolved_permission_request_id[0]);

  manager->EnqueuePermissionResponse(kRequestingOrigin1,
                                     PermissionType::MIDI_SYSEX, true);

  ASSERT_EQ(3u, resolved_permission_status.size());
  EXPECT_EQ(PermissionStatus::GRANTED, resolved_permission_status[1]);
  EXPECT_EQ(1, resolved_permission_request_id[1]);
  EXPECT_EQ(PermissionStatus::GRANTED, resolved_permission_status[2]);
  EXPECT_EQ(1, resolved_permission_request_id[2]);

  // In the second case, the permission A is a type that calls an internal
  // delegate method.
  std::vector<PermissionType> permissions_2 = {PermissionType::GEOLOCATION,
                                               PermissionType::MIDI_SYSEX};

  int request_3 = manager->RequestPermissions(
      permissions_2, render_frame_host, GURL(kRequestingOrigin1), true,
      base::BindOnce(&AwPermissionManagerTest::PermissionsRequestResponse,
                     base::Unretained(this), 3));
  EXPECT_NE(kNoPendingOperation, request_3);
  ASSERT_EQ(3u, resolved_permission_status.size());

  // The permission A is resolved, but the first request isn't finished.
  manager->EnqueuePermissionResponse(kRequestingOrigin1,
                                     PermissionType::GEOLOCATION, false);
  ASSERT_EQ(3u, resolved_permission_status.size());

  // The second request could be resolved synchronously even if the first
  // request isn't finished.
  manager->EnqueuePermissionResponse(kRequestingOrigin2,
                                     PermissionType::GEOLOCATION, true);
  int request_4 = manager->RequestPermission(
      PermissionType::GEOLOCATION, render_frame_host, GURL(kRequestingOrigin2),
      true,
      base::BindOnce(&AwPermissionManagerTest::PermissionRequestResponse,
                     base::Unretained(this), 4));
  EXPECT_EQ(kNoPendingOperation, request_4);
  ASSERT_EQ(4u, resolved_permission_status.size());
  EXPECT_EQ(PermissionStatus::GRANTED, resolved_permission_status[3]);
  EXPECT_EQ(4, resolved_permission_request_id[3]);

  // The first request is finished.
  manager->EnqueuePermissionResponse(kRequestingOrigin1,
                                     PermissionType::MIDI_SYSEX, true);

  ASSERT_EQ(6u, resolved_permission_status.size());
  EXPECT_EQ(PermissionStatus::DENIED, resolved_permission_status[4]);
  EXPECT_EQ(3, resolved_permission_request_id[4]);
  EXPECT_EQ(PermissionStatus::GRANTED, resolved_permission_status[5]);
  EXPECT_EQ(3, resolved_permission_request_id[5]);
}

// Test the case multiple permissions are requested multiple times as follow.
//  1. Permission A and B are requested.
//  2. Permission B is resolved.
//  3. Permission A is requested for the same origin before the A is resolved.
TEST_F(AwPermissionManagerTest, ComplicatedRequestScenario3) {
  // In the first case, the permission A is a type that does not call an
  // internal delegate method.
  std::vector<PermissionType> permissions_1 = {PermissionType::MIDI,
                                               PermissionType::MIDI_SYSEX};

  int request_1 = manager->RequestPermissions(
      permissions_1, render_frame_host, GURL(kRequestingOrigin1), true,
      base::BindOnce(&AwPermissionManagerTest::PermissionsRequestResponse,
                     base::Unretained(this), 1));
  EXPECT_NE(kNoPendingOperation, request_1);
  EXPECT_EQ(0u, resolved_permission_status.size());

  int request_2 = manager->RequestPermission(
      PermissionType::MIDI_SYSEX, render_frame_host, GURL(kRequestingOrigin1),
      true,
      base::BindOnce(&AwPermissionManagerTest::PermissionRequestResponse,
                     base::Unretained(this), 2));
  EXPECT_NE(kNoPendingOperation, request_2);
  EXPECT_EQ(0u, resolved_permission_status.size());

  // Resolving the first request results in both requests finished.
  manager->EnqueuePermissionResponse(kRequestingOrigin1,
                                     PermissionType::MIDI_SYSEX, false);
  ASSERT_EQ(3u, resolved_permission_status.size());
  // Note: The result order in the same requiest is ensured, but each results
  // for a request can be swapped because the manager use IDMap to resolve
  // matched requests.
  EXPECT_EQ(PermissionStatus::GRANTED, resolved_permission_status[1]);
  EXPECT_EQ(1, resolved_permission_request_id[1]);
  EXPECT_EQ(PermissionStatus::DENIED, resolved_permission_status[2]);
  EXPECT_EQ(1, resolved_permission_request_id[2]);
  EXPECT_EQ(PermissionStatus::DENIED, resolved_permission_status[0]);
  EXPECT_EQ(2, resolved_permission_request_id[0]);

  // In the second case, the permission A is a type that calls an internal
  // delegate method.
  std::vector<PermissionType> permissions_2 = {PermissionType::GEOLOCATION,
                                               PermissionType::MIDI_SYSEX};

  int request_3 = manager->RequestPermissions(
      permissions_2, render_frame_host, GURL(kRequestingOrigin1), true,
      base::BindOnce(&AwPermissionManagerTest::PermissionsRequestResponse,
                     base::Unretained(this), 3));
  EXPECT_NE(kNoPendingOperation, request_3);
  ASSERT_EQ(3u, resolved_permission_status.size());

  // The permission B is resolved, but the first request isn't finished.
  manager->EnqueuePermissionResponse(kRequestingOrigin1,
                                     PermissionType::GEOLOCATION, false);
  ASSERT_EQ(3u, resolved_permission_status.size());

  int request_4 = manager->RequestPermission(
      PermissionType::MIDI_SYSEX, render_frame_host, GURL(kRequestingOrigin1),
      true,
      base::BindOnce(&AwPermissionManagerTest::PermissionRequestResponse,
                     base::Unretained(this), 4));
  EXPECT_NE(kNoPendingOperation, request_4);
  ASSERT_EQ(3u, resolved_permission_status.size());

  // Resolving the first request results in both requests finished.
  manager->EnqueuePermissionResponse(kRequestingOrigin1,
                                     PermissionType::MIDI_SYSEX, true);
  ASSERT_EQ(6u, resolved_permission_status.size());
  // Order can be swapped. See Note in ComplicatedRequestScenario1.
  EXPECT_EQ(PermissionStatus::DENIED, resolved_permission_status[4]);
  EXPECT_EQ(3, resolved_permission_request_id[4]);
  EXPECT_EQ(PermissionStatus::GRANTED, resolved_permission_status[5]);
  EXPECT_EQ(3, resolved_permission_request_id[5]);
  EXPECT_EQ(PermissionStatus::GRANTED, resolved_permission_status[3]);
  EXPECT_EQ(4, resolved_permission_request_id[3]);
}

// Test the case multiple permissions are requested multiple times as follow.
//  1. Permission A and B are requested.
//  2. Permission B is resolved.
//  3. Permission A is requested for a different origin before the A is
//     resolved.
TEST_F(AwPermissionManagerTest, ComplicatedRequestScenario4) {
  // In the first case, the permission A is a type that does not call an
  // internal delegate method.
  std::vector<PermissionType> permissions_1 = {PermissionType::MIDI,
                                               PermissionType::MIDI_SYSEX};

  int request_1 = manager->RequestPermissions(
      permissions_1, render_frame_host, GURL(kRequestingOrigin1), true,
      base::BindOnce(&AwPermissionManagerTest::PermissionsRequestResponse,
                     base::Unretained(this), 1));
  EXPECT_NE(kNoPendingOperation, request_1);
  EXPECT_EQ(0u, resolved_permission_status.size());

  int request_2 = manager->RequestPermission(
      PermissionType::MIDI_SYSEX, render_frame_host, GURL(kRequestingOrigin2),
      true,
      base::BindOnce(&AwPermissionManagerTest::PermissionRequestResponse,
                     base::Unretained(this), 2));
  EXPECT_NE(kNoPendingOperation, request_2);
  EXPECT_EQ(0u, resolved_permission_status.size());

  // The second request could be resolved synchronously even if the first
  // request isn't finished.
  manager->EnqueuePermissionResponse(kRequestingOrigin2,
                                     PermissionType::MIDI_SYSEX, true);
  ASSERT_EQ(1u, resolved_permission_status.size());
  EXPECT_EQ(PermissionStatus::GRANTED, resolved_permission_status[0]);
  EXPECT_EQ(2, resolved_permission_request_id[0]);

  // The first request is finished.
  manager->EnqueuePermissionResponse(kRequestingOrigin1,
                                     PermissionType::MIDI_SYSEX, false);
  ASSERT_EQ(3u, resolved_permission_status.size());
  EXPECT_EQ(PermissionStatus::GRANTED, resolved_permission_status[1]);
  EXPECT_EQ(1, resolved_permission_request_id[1]);
  EXPECT_EQ(PermissionStatus::DENIED, resolved_permission_status[2]);
  EXPECT_EQ(1, resolved_permission_request_id[2]);

  // In the second case, the permission A is a type that calls an internal
  // delegate method.
  std::vector<PermissionType> permissions_2 = {PermissionType::GEOLOCATION,
                                               PermissionType::MIDI_SYSEX};

  int request_3 = manager->RequestPermissions(
      permissions_2, render_frame_host, GURL(kRequestingOrigin1), true,
      base::BindOnce(&AwPermissionManagerTest::PermissionsRequestResponse,
                     base::Unretained(this), 3));
  EXPECT_NE(kNoPendingOperation, request_3);
  ASSERT_EQ(3u, resolved_permission_status.size());

  // The permission B is resolved, but the first request isn't finished.
  manager->EnqueuePermissionResponse(kRequestingOrigin1,
                                     PermissionType::GEOLOCATION, false);
  ASSERT_EQ(3u, resolved_permission_status.size());

  int request_4 = manager->RequestPermission(
      PermissionType::MIDI_SYSEX, render_frame_host, GURL(kRequestingOrigin2),
      true,
      base::BindOnce(&AwPermissionManagerTest::PermissionRequestResponse,
                     base::Unretained(this), 4));
  EXPECT_NE(kNoPendingOperation, request_4);
  ASSERT_EQ(3u, resolved_permission_status.size());

  // The second request could be resolved synchronously even if the first
  // request isn't finished.
  manager->EnqueuePermissionResponse(kRequestingOrigin2,
                                     PermissionType::MIDI_SYSEX, false);
  ASSERT_EQ(4u, resolved_permission_status.size());
  EXPECT_EQ(PermissionStatus::DENIED, resolved_permission_status[3]);
  EXPECT_EQ(4, resolved_permission_request_id[3]);

  // Resolving the first request results in resuming the second request.
  manager->EnqueuePermissionResponse(kRequestingOrigin1,
                                     PermissionType::MIDI_SYSEX, false);
  ASSERT_EQ(6u, resolved_permission_status.size());
  EXPECT_EQ(PermissionStatus::DENIED, resolved_permission_status[4]);
  EXPECT_EQ(3, resolved_permission_request_id[4]);
  EXPECT_EQ(PermissionStatus::DENIED, resolved_permission_status[5]);
  EXPECT_EQ(3, resolved_permission_request_id[5]);
}

}  // namespace

}  // namespace android_webview
