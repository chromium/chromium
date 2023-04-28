// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_disk_manager_dispatcher.h"

#include "chrome/browser/ash/borealis/borealis_disk_manager.h"
#include "chrome/browser/ash/borealis/borealis_disk_manager_impl.h"
#include "chrome/browser/ash/borealis/borealis_metrics.h"
#include "chrome/browser/ash/borealis/testing/callback_factory.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace borealis {
namespace {

class DiskManagerMock : public BorealisDiskManager {
 public:
  DiskManagerMock() = default;
  ~DiskManagerMock() override = default;
  MOCK_METHOD(void,
              GetDiskInfo,
              (base::OnceCallback<
                  void(base::expected<GetDiskInfoResponse,
                                      Described<BorealisGetDiskInfoResult>>)>),
              ());
  MOCK_METHOD(
      void,
      RequestSpace,
      (uint64_t,
       base::OnceCallback<void(
           base::expected<uint64_t, Described<BorealisResizeDiskResult>>)>),
      ());
  MOCK_METHOD(
      void,
      ReleaseSpace,
      (uint64_t,
       base::OnceCallback<void(
           base::expected<uint64_t, Described<BorealisResizeDiskResult>>)>),
      ());
  MOCK_METHOD(void,
              SyncDiskSize,
              (base::OnceCallback<
                  void(base::expected<BorealisSyncDiskSizeResult,
                                      Described<BorealisSyncDiskSizeResult>>)>),
              ());
};

using DiskInfoCallbackFactory = NiceCallbackFactory<void(
    base::expected<BorealisDiskManagerImpl::GetDiskInfoResponse,
                   Described<BorealisGetDiskInfoResult>>)>;

using RequestDeltaCallbackFactory = NiceCallbackFactory<void(
    base::expected<uint64_t, Described<BorealisResizeDiskResult>>)>;

TEST(BorealisDiskManagerDispatcherTest, GetDiskInfoFailsIfNamesDontMatch) {
  BorealisDiskManagerDispatcher dispatcher;
  DiskManagerMock disk_mock;

  DiskInfoCallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(testing::_))
      .WillOnce(testing::Invoke(
          [](base::expected<BorealisDiskManagerImpl::GetDiskInfoResponse,
                            Described<BorealisGetDiskInfoResult>>
                 response_or_error) {
            EXPECT_FALSE(response_or_error.has_value());
            EXPECT_EQ(response_or_error.error().error(),
                      BorealisGetDiskInfoResult::kInvalidRequest);
          }));

  dispatcher.SetDiskManagerDelegate(&disk_mock);
  dispatcher.GetDiskInfo("NOTBOREALIS", "penguin", callback_factory.BindOnce());
}

TEST(BorealisDiskManagerDispatcherTest, GetDiskInfoFailsIfDelegateNotSet) {
  BorealisDiskManagerDispatcher dispatcher;
  DiskInfoCallbackFactory callback_factory;

  EXPECT_CALL(callback_factory, Call(testing::_))
      .WillOnce(testing::Invoke(
          [](base::expected<BorealisDiskManagerImpl::GetDiskInfoResponse,
                            Described<BorealisGetDiskInfoResult>>
                 response_or_error) {
            EXPECT_FALSE(response_or_error.has_value());
            EXPECT_EQ(response_or_error.error().error(),
                      BorealisGetDiskInfoResult::kInvalidRequest);
          }));

  dispatcher.GetDiskInfo("borealis", "penguin", callback_factory.BindOnce());
}

TEST(BorealisDiskManagerDispatcherTest, GetDiskInfoSucceedsAndCallsDelegate) {
  BorealisDiskManagerDispatcher dispatcher;
  DiskManagerMock disk_mock;

  DiskInfoCallbackFactory callback_factory;
  EXPECT_CALL(disk_mock, GetDiskInfo(testing::_));

  dispatcher.SetDiskManagerDelegate(&disk_mock);
  dispatcher.GetDiskInfo("borealis", "penguin", callback_factory.BindOnce());
}

TEST(BorealisDiskManagerDispatcherTest, RequestSpaceFailsIfNamesDontMatch) {
  BorealisDiskManagerDispatcher dispatcher;
  DiskManagerMock disk_mock;

  RequestDeltaCallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(testing::_))
      .WillOnce(testing::Invoke(
          [](base::expected<uint64_t, Described<BorealisResizeDiskResult>>
                 response_or_error) {
            EXPECT_FALSE(response_or_error.has_value());
            EXPECT_EQ(response_or_error.error().error(),
                      BorealisResizeDiskResult::kInvalidRequest);
          }));

  dispatcher.SetDiskManagerDelegate(&disk_mock);
  dispatcher.RequestSpace("borealis", "NOTPENGUIN", 1,
                          callback_factory.BindOnce());
}

TEST(BorealisDiskManagerDispatcherTest, RequestSpaceFailsIfDelegateNotSet) {
  BorealisDiskManagerDispatcher dispatcher;
  RequestDeltaCallbackFactory callback_factory;

  EXPECT_CALL(callback_factory, Call(testing::_))
      .WillOnce(testing::Invoke(
          [](base::expected<uint64_t, Described<BorealisResizeDiskResult>>
                 response_or_error) {
            EXPECT_FALSE(response_or_error.has_value());
            EXPECT_EQ(response_or_error.error().error(),
                      BorealisResizeDiskResult::kInvalidRequest);
          }));

  dispatcher.RequestSpace("borealis", "penguin", 1,
                          callback_factory.BindOnce());
}

TEST(BorealisDiskManagerDispatcherTest, RequestSpaceSucceedsAndCallsDelegate) {
  BorealisDiskManagerDispatcher dispatcher;
  DiskManagerMock disk_mock;

  RequestDeltaCallbackFactory callback_factory;
  EXPECT_CALL(disk_mock, RequestSpace(1, testing::_));

  dispatcher.SetDiskManagerDelegate(&disk_mock);
  dispatcher.RequestSpace("borealis", "penguin", 1,
                          callback_factory.BindOnce());
}

TEST(BorealisDiskManagerDispatcherTest, ReleaseSpaceFailsIfNamesDontMatch) {
  BorealisDiskManagerDispatcher dispatcher;
  DiskManagerMock disk_mock;

  RequestDeltaCallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(testing::_))
      .WillOnce(testing::Invoke(
          [](base::expected<uint64_t, Described<BorealisResizeDiskResult>>
                 response_or_error) {
            EXPECT_FALSE(response_or_error.has_value());
            EXPECT_EQ(response_or_error.error().error(),
                      BorealisResizeDiskResult::kInvalidRequest);
          }));

  dispatcher.SetDiskManagerDelegate(&disk_mock);
  dispatcher.ReleaseSpace("NOTBOREALIS", "NOTPENGUIN", 1,
                          callback_factory.BindOnce());
}

TEST(BorealisDiskManagerDispatcherTest, ReleaseSpaceFailsIfDelegateNotSet) {
  BorealisDiskManagerDispatcher dispatcher;
  RequestDeltaCallbackFactory callback_factory;

  EXPECT_CALL(callback_factory, Call(testing::_))
      .WillOnce(testing::Invoke(
          [](base::expected<uint64_t, Described<BorealisResizeDiskResult>>
                 response_or_error) {
            EXPECT_FALSE(response_or_error.has_value());
            EXPECT_EQ(response_or_error.error().error(),
                      BorealisResizeDiskResult::kInvalidRequest);
          }));

  dispatcher.ReleaseSpace("borealis", "penguin", 1,
                          callback_factory.BindOnce());
}

TEST(BorealisDiskManagerDispatcherTest, ReleaseSpaceSucceedsAndCallsDelegate) {
  BorealisDiskManagerDispatcher dispatcher;
  DiskManagerMock disk_mock;

  RequestDeltaCallbackFactory callback_factory;
  EXPECT_CALL(disk_mock, ReleaseSpace(1, testing::_));

  dispatcher.SetDiskManagerDelegate(&disk_mock);
  dispatcher.ReleaseSpace("borealis", "penguin", 1,
                          callback_factory.BindOnce());
}

}  // namespace
}  // namespace borealis
